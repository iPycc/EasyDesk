#include "sessions/files/session_file_receive_worker.h"

#include "files/receive_directory.h"
#include "network/files/protocol/file_receipt_codec.h"
#include "sessions/files/session_file_receiver.h"

#include <QMetaObject>
#include <QPointer>

namespace {
constexpr qsizetype kMaximumBufferedBytes = 2 * 1024 * 1024;
constexpr int kMaximumFrameBytes = 16384;
}

SessionFileReceiveWorker::SessionFileReceiveWorker(QObject *parent) : QObject(parent) {}

SessionFileReceiveWorker::~SessionFileReceiveWorker() {
    stop();
    m_thread.wait();
}

bool SessionFileReceiveWorker::start(
    const easydesk::client::transfer::TransferManifest &manifest,
    std::shared_ptr<ReceiveDirectory> directory, QString &error) {
    if (QThread::currentThread() != thread() || m_active || m_thread.isRunning()) {
        error = QStringLiteral("File receive worker is unavailable.");
        return false;
    }
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    auto receiver = SessionFileReceiver::create(manifest, std::move(directory), error, cancellation);
    if (!receiver)
        return false;
    m_cancellation = std::move(cancellation);
    m_receiver = std::shared_ptr<SessionFileReceiver>(std::move(receiver));
    m_dispatcher = new QObject;
    m_dispatcher->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_dispatcher, &QObject::deleteLater);
    m_active = true;
    m_busy = false;
    m_bufferedBytes = 0;
    m_thread.start();
    error.clear();
    return true;
}

bool SessionFileReceiveWorker::enqueue(const QByteArray &frame, QString &error) {
    if (QThread::currentThread() != thread() || !m_active || frame.isEmpty() ||
        frame.size() > kMaximumFrameBytes || frame.size() > kMaximumBufferedBytes - m_bufferedBytes) {
        error = QStringLiteral("File receive queue is unavailable or full.");
        return false;
    }
    m_bufferedBytes += frame.size();
    m_frames.enqueue(frame);
    dispatchNext();
    error.clear();
    return true;
}

void SessionFileReceiveWorker::stop() {
    if (m_cancellation)
        m_cancellation->store(true, std::memory_order_release);
    m_active = false;
    m_frames.clear();
    m_bufferedBytes = 0;
    m_busy = false;
    m_receiver.reset();
    m_dispatcher = nullptr;
    m_thread.quit();
}

void SessionFileReceiveWorker::dispatchNext() {
    if (!m_active || m_busy || m_frames.isEmpty() || !m_dispatcher)
        return;
    m_busy = true;
    const QByteArray frame = m_frames.dequeue();
    const auto receiver = m_receiver;
    const auto cancellation = m_cancellation;
    QMetaObject::invokeMethod(m_dispatcher, [this, receiver, cancellation, frame] {
        QString name, error;
        easydesk::client::transfer::FileReceipt receipt;
        const auto result = receiver->receive(frame, name, error, &receipt);
        QByteArray encoded;
        const bool okay = result != SessionFileReceiver::Result::Failed &&
                          easydesk::client::transfer::encodeFileReceipt(receipt, encoded, error);
        QMetaObject::invokeMethod(this, [this, cancellation, size = frame.size(), result, name, error, encoded, okay] {
            if (!m_active || m_cancellation != cancellation || cancellation->load(std::memory_order_acquire))
                return;
            m_bufferedBytes -= size;
            m_busy = false;
            if (!okay) {
                stop();
                emit failed(error.isEmpty() ? QStringLiteral("File receive failed.") : error);
                return;
            }
            const bool complete = result == SessionFileReceiver::Result::BatchCompleted;
            if (complete && !m_frames.isEmpty()) {
                stop();
                emit failed(QStringLiteral("Unexpected data after the final file."));
                return;
            }
            if (complete) {
                m_active = false;
                m_receiver.reset();
                m_dispatcher = nullptr;
                m_thread.quit();
            }
            QPointer<SessionFileReceiveWorker> guard(this);
            emit receiptReady(encoded);
            if (!guard || (!complete && !m_active) || m_cancellation != cancellation ||
                cancellation->load(std::memory_order_acquire)) return;
            if (result == SessionFileReceiver::Result::FileCommitted || complete) {
                emit fileCommitted(name);
                if (!guard || (!complete && !m_active) || m_cancellation != cancellation ||
                    cancellation->load(std::memory_order_acquire)) return;
            }
            if (complete) {
                emit batchCompleted();
            } else {
                dispatchNext();
            }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
