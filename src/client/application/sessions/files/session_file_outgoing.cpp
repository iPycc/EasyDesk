#include "sessions/files/session_file_outgoing.h"

#include "sessions/files/session_transfer_offer.h"
#include "data/settings/identity/trusted_peer_store.h"
#include "sessions/negotiation/session_negotiation_coordinator.h"

namespace {
constexpr int kRetryMilliseconds = 25;
constexpr int kProgressTimeoutMilliseconds = 30000;
constexpr int kMaximumPendingReceipts = 1024;
}

SessionFileOutgoing::SessionFileOutgoing(SessionNegotiationCoordinator &session,
                                         SessionTransferOffer &offer, TrustedPeerStore &trustedPeers,
                                         QObject *parent)
    : QObject(parent), m_session(session), m_offer(offer), m_trustedPeers(trustedPeers) {
    m_retry.setSingleShot(true);
    m_timeout.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &SessionFileOutgoing::sendFrame);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("File transfer timed out.")); });
    connect(&m_worker, &SessionFileSendWorker::prepared, this, [this](const auto &manifest) {
        if (m_state != State::Preparing) return;
        m_transferId = manifest.transferId;
        QString error;
        if (!m_offer.sendOffer(manifest, error)) { fail(error); return; }
        m_state = State::AwaitingDecision;
        emit awaitingDecision();
    });
    connect(&m_offer, &SessionTransferOffer::decisionReceived, this, [this](const auto &decision) {
        if (m_state != State::AwaitingDecision || decision.transferId != m_transferId) return;
        if (decision.decision == easydesk::client::transfer::OfferDecision::Decline) {
            stop();
            emit declined();
            return;
        }
        if (!m_session.setFileSendingEnabled(true) || !m_worker.begin(decision.windowBytes)) {
            fail(QStringLiteral("File send channel is unavailable."));
            return;
        }
        m_state = State::Sending;
        m_timeout.start(kProgressTimeoutMilliseconds);
    });
    connect(&m_offer, &SessionTransferOffer::outgoingCancelled, this,
            [this](const QByteArray &id, auto) {
                if (m_state == State::Idle || id != m_transferId) return;
                stop();
                emit cancelled();
            });
    connect(&m_offer, &SessionTransferOffer::failed, this, [this](const QString &error) {
        if (m_state != State::Idle) fail(error);
    });
    connect(&m_session, &SessionNegotiationCoordinator::fileAvailabilityChanged, this, [this] {
        if (m_state != State::Idle && (!m_session.isFileChannelOpen(FileChannel::Metadata) ||
                                       !m_session.isFileChannelOpen(FileChannel::Data)))
            fail(QStringLiteral("File session was interrupted."));
    });
    connect(&m_session, &SessionNegotiationCoordinator::stateChanged, this,
            [this](SessionNegotiationCoordinator::State state) {
                if (m_state != State::Idle && (state == SessionNegotiationCoordinator::State::Idle ||
                                               state == SessionNegotiationCoordinator::State::Ending))
                    fail(QStringLiteral("File session was interrupted."));
            });
    connect(&m_session, &SessionNegotiationCoordinator::fileMessageReceived, this,
            [this](FileChannel channel, const QByteArray &message) {
                using namespace easydesk::client::transfer;
                if (channel != FileChannel::Metadata || !isFileReceipt(message)) return;
                if (m_state == State::Cancelling) return;
                if (m_state != State::Sending || m_receiptsPending >= kMaximumPendingReceipts) {
                    if (m_state != State::Idle) fail(QStringLiteral("Unexpected file receipt."));
                    return;
                }
                FileReceipt receipt;
                QString error;
                if (!decodeFileReceipt(message, receipt, error) || !m_worker.applyReceipt(receipt)) {
                    fail(error.isEmpty() ? QStringLiteral("File receipt could not be processed.") : error);
                    return;
                }
                ++m_receiptsPending;
                m_timeout.start(kProgressTimeoutMilliseconds);
            });
    connect(&m_worker, &SessionFileSendWorker::receiptProcessed, this, [this] {
        if (m_receiptsPending > 0) --m_receiptsPending;
    });
    connect(&m_worker, &SessionFileSendWorker::readinessChanged, this, [this](bool ready) {
        if (ready) requestFrame();
    });
    connect(&m_worker, &SessionFileSendWorker::frameReady, this, [this](const QByteArray &frame) {
        if (m_state != State::Sending) return;
        m_frameRequested = false;
        if (frame.isEmpty() || !m_pendingFrame.isEmpty()) { fail(QStringLiteral("File frame state is invalid.")); return; }
        m_pendingFrame = frame;
        sendFrame();
    });
    connect(&m_worker, &SessionFileSendWorker::progress, this, [this](quint64 queued, quint64 verified) {
        if (m_state == State::Sending) {
            m_timeout.start(kProgressTimeoutMilliseconds);
            emit progress(queued, verified);
        }
    });
    connect(&m_worker, &SessionFileSendWorker::fileCommitted, this, &SessionFileOutgoing::fileCommitted);
    connect(&m_worker, &SessionFileSendWorker::failed, this, &SessionFileOutgoing::fail);
    connect(&m_worker, &SessionFileSendWorker::batchCompleted, this, [this] {
        if (m_state != State::Sending) return;
        stop();
        emit batchCompleted();
    });
}

SessionFileOutgoing::~SessionFileOutgoing() { stop(); }

bool SessionFileOutgoing::start(const QStringList &paths, QString &error) {
    if (m_state != State::Idle || !m_session.isSessionReady() || !m_session.context().peerMayReceiveFiles ||
        !m_session.isFileChannelOpen(FileChannel::Metadata) || !m_session.isFileChannelOpen(FileChannel::Data)) {
        error = QStringLiteral("File session is unavailable.");
        return false;
    }
    const auto context = m_session.context();
    if (m_trustedPeers.status(context.peerDeviceId, context.peerPublicKey) != TrustedPeerStore::Status::Matched) {
        error = QStringLiteral("Peer identity has not been verified for this device.");
        return false;
    }
    if (!m_worker.prepare(paths, error)) return false;
    m_state = State::Preparing;
    error.clear();
    emit preparing();
    return true;
}

bool SessionFileOutgoing::cancel(QString &error) {
    if (m_state == State::Preparing) {
        stop();
        error.clear();
        emit cancelled();
        return true;
    }
    if (m_state == State::Idle) {
        error = QStringLiteral("No outgoing transfer is active.");
        return false;
    }
    if (!m_offer.cancelOutgoing(easydesk::client::transfer::TransferCancelReason::SenderCancelled, error))
        return false;
    if (m_state != State::Idle) {
        m_state = State::Cancelling;
        m_retry.stop();
        m_timeout.stop();
        m_session.setFileSendingEnabled(false);
        m_worker.stop();
        m_pendingFrame.clear();
        m_frameRequested = false;
    }
    return true;
}

void SessionFileOutgoing::stop() {
    m_retry.stop();
    m_timeout.stop();
    m_session.setFileSendingEnabled(false);
    m_worker.stop();
    m_offer.finishOutgoing(m_transferId);
    m_transferId.clear();
    m_pendingFrame.clear();
    m_receiptsPending = 0;
    m_frameRequested = false;
    m_state = State::Idle;
}

void SessionFileOutgoing::requestFrame() {
    if (m_state != State::Sending || m_frameRequested || !m_pendingFrame.isEmpty()) return;
    if (!m_worker.requestFrame()) { fail(QStringLiteral("File frame could not be prepared.")); return; }
    m_frameRequested = true;
}

void SessionFileOutgoing::sendFrame() {
    if (m_state != State::Sending || m_pendingFrame.isEmpty()) return;
    if (!m_session.isSessionReady()) { fail(QStringLiteral("File session was interrupted.")); return; }
    const auto result = m_session.sendFileMessage(FileChannel::Data, m_pendingFrame);
    if (result == SessionFileTransport::SendResult::WouldBlock) { m_retry.start(kRetryMilliseconds); return; }
    if (result != SessionFileTransport::SendResult::Sent || !m_worker.markQueued()) {
        fail(QStringLiteral("File frame could not be sent."));
        return;
    }
    m_pendingFrame.clear();
    m_timeout.start(kProgressTimeoutMilliseconds);
}

void SessionFileOutgoing::fail(const QString &error) {
    if (m_state == State::Idle) return;
    stop();
    emit failed(error);
}
