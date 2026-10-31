#include "network/winhttp/winhttp_websocket_transport.h"

#include <QMetaObject>
#include <QTimer>
#include <utility>

namespace {

constexpr qsizetype kMaximumQueuedMessages = 64;
constexpr qsizetype kMaximumQueuedBytes = 1024 * 1024;
constexpr qsizetype kMaximumMessageSize = 64 * 1024;

}  // namespace

WinHttpWebSocketTransport::WinHttpWebSocketTransport(QObject *parent)
    : QObject(parent), m_sendThread([this] { sendLoop(); }) {}

WinHttpWebSocketTransport::~WinHttpWebSocketTransport() {
    m_stopRequested.store(true, std::memory_order_release);
    clearSendQueue();
    m_connection.abort();
    if (m_receiveThread.joinable())
        m_receiveThread.join();
    {
        std::lock_guard lock(m_sendQueueMutex);
        m_senderStopping = true;
        m_shutdownQueued = false;
        m_sendQueue.clear();
        m_queuedBytes = 0;
    }
    m_sendQueueChanged.notify_all();
    if (m_sendThread.joinable())
        m_sendThread.join();
}

WinHttpWebSocketTransport::State WinHttpWebSocketTransport::state() const {
    return m_state.load(std::memory_order_acquire);
}

bool WinHttpWebSocketTransport::connectToServer(const QUrl &endpoint,
                                                bool allowInsecureLoopback) {
    if (state() != State::Disconnected) {
        emit errorOccurred(QStringLiteral("Signaling transport is already active."));
        return false;
    }
    if (m_receiveThread.joinable())
        m_receiveThread.join();

    m_stopRequested.store(false, std::memory_order_release);
    m_state.store(State::Connecting, std::memory_order_release);
    emit stateChanged(State::Connecting);
    m_receiveThread = std::thread(
        [this, endpoint, allowInsecureLoopback] {
            receiveLoop(endpoint, allowInsecureLoopback);
        });
    return true;
}

bool WinHttpWebSocketTransport::sendText(const QByteArray &message) {
    if (message.size() > kMaximumMessageSize) {
        emit errorOccurred(QStringLiteral("Signaling message exceeds the 64 KiB limit."));
        return false;
    }
    if (state() != State::Connected) {
        emit errorOccurred(QStringLiteral("Signaling transport is not connected."));
        return false;
    }

    bool queueFull = false;
    {
        std::lock_guard lock(m_sendQueueMutex);
        if (state() != State::Connected || m_senderStopping)
            return false;
        if (static_cast<qsizetype>(m_sendQueue.size()) >= kMaximumQueuedMessages ||
            m_queuedBytes + message.size() > kMaximumQueuedBytes) {
            queueFull = true;
        } else {
            m_sendQueue.push_back(message);
            m_queuedBytes += message.size();
        }
    }
    if (queueFull) {
        emit errorOccurred(QStringLiteral("Signaling send queue is full."));
        return false;
    }
    m_sendQueueChanged.notify_one();
    return true;
}

void WinHttpWebSocketTransport::disconnectFromServer() {
    const State current = state();
    if (current == State::Disconnected || current == State::Closing)
        return;
    m_stopRequested.store(true, std::memory_order_release);
    m_state.store(State::Closing, std::memory_order_release);
    emit stateChanged(State::Closing);
    clearSendQueue();
    if (current == State::Connecting) {
        m_connection.abort();
        return;
    }

    {
        std::lock_guard lock(m_sendQueueMutex);
        m_shutdownQueued = true;
    }
    m_sendQueueChanged.notify_one();
    QTimer::singleShot(3000, this, [this] {
        if (state() == State::Closing)
            m_connection.abort();
    });
}

void WinHttpWebSocketTransport::receiveLoop(QUrl endpoint,
                                            bool allowInsecureLoopback) {
    QString error;
    if (!m_connection.open(endpoint, allowInsecureLoopback, error)) {
        if (!m_stopRequested.load(std::memory_order_acquire))
            postError(std::move(error));
        postState(State::Disconnected);
        return;
    }

    postState(State::Connected);
    while (true) {
        WinHttpWebSocketReceiveResult result;
        if (!m_connection.receive(result, error)) {
            if (!m_stopRequested.load(std::memory_order_acquire))
                postError(std::move(error));
            break;
        }
        if (result.kind == WinHttpWebSocketReceiveResult::Kind::Closed) {
            QMetaObject::invokeMethod(
                this,
                [this, status = result.closeStatus, reason = std::move(result.closeReason)] {
                    emit remoteClosed(status, reason);
                },
                Qt::QueuedConnection);
            break;
        }
        QMetaObject::invokeMethod(
            this,
            [this, message = std::move(result.payload)] {
                emit textReceived(message);
            },
            Qt::QueuedConnection);
    }

    m_stopRequested.store(true, std::memory_order_release);
    m_connection.abort();
    clearSendQueue();
    postState(State::Disconnected);
}

void WinHttpWebSocketTransport::sendLoop() {
    while (true) {
        QByteArray message;
        bool shutdown = false;
        {
            std::unique_lock lock(m_sendQueueMutex);
            m_sendQueueChanged.wait(lock, [this] {
                return m_senderStopping || m_shutdownQueued || !m_sendQueue.empty();
            });
            if (m_senderStopping)
                return;
            if (m_shutdownQueued) {
                m_shutdownQueued = false;
                shutdown = true;
            } else {
                message = std::move(m_sendQueue.front());
                m_sendQueue.pop_front();
                m_queuedBytes -= message.size();
            }
        }

        if (shutdown) {
            QString error;
            if (!m_connection.shutdown(error))
                m_connection.abort();
            continue;
        }

        if (state() != State::Connected)
            continue;
        QString error;
        if (!m_connection.sendText(message, error)) {
            if (m_stopRequested.load(std::memory_order_acquire))
                continue;
            m_stopRequested.store(true, std::memory_order_release);
            postError(std::move(error));
            postState(State::Closing);
            clearSendQueue();
            m_connection.abort();
        }
    }
}

void WinHttpWebSocketTransport::clearSendQueue() {
    std::lock_guard lock(m_sendQueueMutex);
    m_sendQueue.clear();
    m_queuedBytes = 0;
}

void WinHttpWebSocketTransport::postState(State state) {
    State previous = m_state.load(std::memory_order_acquire);
    if (state == State::Connected) {
        previous = State::Connecting;
        if (!m_state.compare_exchange_strong(previous, State::Connected,
                                             std::memory_order_acq_rel))
            return;
    } else if (state == State::Closing) {
        while (previous != State::Disconnected && previous != State::Closing &&
               !m_state.compare_exchange_weak(previous, State::Closing,
                                              std::memory_order_acq_rel)) {
        }
        if (previous == State::Disconnected || previous == State::Closing)
            return;
    } else {
        previous = m_state.exchange(state, std::memory_order_acq_rel);
        if (previous == state)
            return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, state] {
            if (m_state.load(std::memory_order_acquire) == state)
                emit stateChanged(state);
        },
        Qt::QueuedConnection);
}

void WinHttpWebSocketTransport::postError(QString error) {
    if (error.isEmpty())
        return;
    QMetaObject::invokeMethod(
        this,
        [this, error = std::move(error)] { emit errorOccurred(error); },
        Qt::QueuedConnection);
}
