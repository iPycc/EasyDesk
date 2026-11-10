#include "sessions/files/session_file_send_worker.h"

#include "files/send_file_source.h"
#include "security/secure_random.h"
#include "sessions/files/session_file_sender.h"

#include <QMetaObject>
#include <QPointer>
#include <vector>

using namespace easydesk::client::transfer;

struct SessionFileSendWorkerState {
    TransferManifest manifest;
    std::vector<std::unique_ptr<SendFileSource>> sources;
    std::unique_ptr<SessionFileSender> sender;
    std::size_t index = 0;
    quint32 windowBytes = 0;
};

SessionFileSendWorker::SessionFileSendWorker(QObject *parent) : QObject(parent) {}
SessionFileSendWorker::~SessionFileSendWorker() { stop(); m_thread.wait(); }

bool SessionFileSendWorker::usable() const {
    return QThread::currentThread() == thread() && m_active && m_dispatcher && m_state &&
           m_cancellation && !m_cancellation->load(std::memory_order_acquire);
}

bool SessionFileSendWorker::prepare(const QStringList &paths, QString &error) {
    if (QThread::currentThread() != thread() || m_active || m_thread.isRunning() ||
        paths.isEmpty() || paths.size() > kMaximumFiles) {
        error = QStringLiteral("File preparation is unavailable or the file count is invalid.");
        return false;
    }
    m_cancellation = std::make_shared<std::atomic_bool>(false);
    m_state = std::make_shared<SessionFileSendWorkerState>();
    m_dispatcher = new QObject;
    m_dispatcher->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_dispatcher, &QObject::deleteLater);
    m_active = true;
    m_frameRequested = false;
    m_framePending = false;
    const auto state = m_state;
    const auto cancellation = m_cancellation;
    m_thread.start();
    QMetaObject::invokeMethod(m_dispatcher, [this, state, cancellation, paths] {
        QString failure;
        state->manifest.transferId = easydesk::client::windows::generateSecureRandomBytes(16, failure);
        state->sources.reserve(static_cast<std::size_t>(paths.size()));
        for (const QString &path : paths) {
            if (cancellation->load(std::memory_order_acquire) || !failure.isEmpty()) break;
            auto source = SendFileSource::open(path, failure, cancellation.get());
            if (!source) break;
            state->manifest.files.append(source->manifest());
            state->sources.push_back(std::move(source));
        }
        quint64 total = 0;
        if (failure.isEmpty() && !cancellation->load(std::memory_order_acquire))
            (void)validateManifest(state->manifest, kDefaultMaximumFileBytes, total, failure);
        QMetaObject::invokeMethod(this, [this, state, cancellation, failure] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire)) return;
            if (!failure.isEmpty()) { fail(failure); return; }
            emit prepared(state->manifest);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    error.clear();
    return true;
}

bool SessionFileSendWorker::begin(quint32 windowBytes) {
    if (!usable()) return false;
    const auto state = m_state;
    const auto cancellation = m_cancellation;
    return QMetaObject::invokeMethod(m_dispatcher, [this, state, cancellation, windowBytes] {
        QString error;
        if (state->sender || state->sources.empty()) error = QStringLiteral("File sender state is invalid.");
        else {
            state->windowBytes = windowBytes;
            state->sender = SessionFileSender::create(state->manifest.transferId,
                                                      std::move(state->sources[0]), error, windowBytes);
        }
        const bool ready = state->sender && state->sender->canQueue();
        QMetaObject::invokeMethod(this, [this, cancellation, ready, error] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire)) return;
            if (!error.isEmpty()) { fail(error); return; }
            emit readinessChanged(ready);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

bool SessionFileSendWorker::requestFrame() {
    if (!usable() || m_frameRequested || m_framePending) return false;
    m_frameRequested = true;
    const auto state = m_state;
    const auto cancellation = m_cancellation;
    const bool scheduled = QMetaObject::invokeMethod(m_dispatcher, [this, state, cancellation] {
        QByteArray frame;
        QString error;
        const bool waiting = state->sender && !state->sender->canQueue() &&
                             state->sender->state() != SessionFileSender::State::Failed &&
                             state->sender->state() != SessionFileSender::State::Completed;
        const bool okay = waiting || (state->sender && state->sender->nextFrame(frame, error));
        QMetaObject::invokeMethod(this, [this, cancellation, frame, error, okay, waiting] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire)) return;
            m_frameRequested = false;
            if (!okay) { fail(error); return; }
            if (waiting) { emit readinessChanged(false); return; }
            m_framePending = true;
            emit frameReady(frame);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    if (!scheduled) m_frameRequested = false;
    return scheduled;
}

bool SessionFileSendWorker::markQueued() {
    if (!usable() || !m_framePending) return false;
    m_framePending = false;
    const auto state = m_state;
    const auto cancellation = m_cancellation;
    return QMetaObject::invokeMethod(m_dispatcher, [this, state, cancellation] {
        QString error;
        const bool okay = state->sender && state->sender->markQueued(error);
        const quint64 queued = okay ? state->sender->queuedBytes() : 0;
        const quint64 verified = okay ? state->sender->verifiedBytes() : 0;
        const bool ready = okay && state->sender->canQueue();
        QMetaObject::invokeMethod(this, [this, cancellation, queued, verified, ready, error, okay] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire)) return;
            if (!okay) { fail(error); return; }
            QPointer<SessionFileSendWorker> guard(this);
            emit progress(queued, verified);
            if (guard && m_active && m_cancellation == cancellation) emit readinessChanged(ready);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

bool SessionFileSendWorker::applyReceipt(const FileReceipt &receipt) {
    if (!usable()) return false;
    const auto state = m_state;
    const auto cancellation = m_cancellation;
    return QMetaObject::invokeMethod(m_dispatcher, [this, state, cancellation, receipt] {
        QString error;
        bool okay = state->sender && state->sender->applyReceipt(receipt, error);
        const quint64 queued = okay ? state->sender->queuedBytes() : 0;
        const quint64 verified = okay ? state->sender->verifiedBytes() : 0;
        const bool committed = okay && state->sender->state() == SessionFileSender::State::Completed;
        bool complete = false;
        if (committed) {
            ++state->index;
            if (state->index == state->sources.size()) complete = true;
            else {
                state->sender = SessionFileSender::create(state->manifest.transferId,
                    std::move(state->sources[state->index]), error, state->windowBytes);
                okay = bool(state->sender);
            }
        }
        const bool ready = okay && !complete && state->sender->canQueue();
        QMetaObject::invokeMethod(this, [this, cancellation, receipt, queued, verified, ready, error, okay, committed, complete] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire)) return;
            if (!okay) { fail(error); return; }
            QPointer<SessionFileSendWorker> guard(this);
            emit receiptProcessed();
            if (!guard || !m_active || m_cancellation != cancellation) return;
            emit progress(queued, verified);
            if (!guard || !m_active || m_cancellation != cancellation) return;
            if (committed) {
                emit fileCommitted(receipt.finalName);
                if (!guard || !m_active || m_cancellation != cancellation) return;
            }
            if (complete) {
                m_active = false;
                m_state.reset();
                m_dispatcher = nullptr;
                m_thread.quit();
                emit batchCompleted();
            } else emit readinessChanged(ready);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void SessionFileSendWorker::stop() {
    if (m_cancellation) m_cancellation->store(true, std::memory_order_release);
    m_active = false;
    m_frameRequested = false;
    m_framePending = false;
    m_state.reset();
    m_dispatcher = nullptr;
    m_thread.quit();
}

void SessionFileSendWorker::fail(const QString &error) {
    stop();
    emit failed(error.isEmpty() ? QStringLiteral("File send worker failed.") : error);
}
