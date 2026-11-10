#include "sessions/files/session_file_incoming.h"

#include "files/receive_directory.h"
#include "data/settings/identity/trusted_peer_store.h"
#include "sessions/files/session_transfer_offer.h"
#include "sessions/negotiation/session_negotiation_coordinator.h"

namespace {
constexpr int kRetryMilliseconds = 25;
constexpr int kReceiptTimeoutMilliseconds = 30000;
constexpr qsizetype kMaximumReceiptBytes = 256 * 1024;
}

SessionFileIncoming::SessionFileIncoming(SessionNegotiationCoordinator &session,
                                         SessionTransferOffer &offer, TrustedPeerStore &trustedPeers,
                                         QObject *parent)
    : QObject(parent), m_session(session), m_offer(offer), m_trustedPeers(trustedPeers) {
    m_retry.setSingleShot(true);
    m_timeout.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &SessionFileIncoming::flushReceipts);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("File receipt timed out.")); });
    connect(&m_offer, &SessionTransferOffer::offerReceived, this,
            [this](const auto &manifest) { if (!m_active) { m_manifest = manifest; m_hasOffer = true; } });
    connect(&m_offer, &SessionTransferOffer::failed, this, [this](const QString &error) {
        if (m_active || m_cancelling) fail(error);
        else { m_hasOffer = false; m_manifest = {}; }
    });
    connect(&m_offer, &SessionTransferOffer::decisionSent, this,
            [this](easydesk::client::transfer::OfferDecision decision) {
                if (decision == easydesk::client::transfer::OfferDecision::Decline && !m_active) {
                    m_hasOffer = false;
                    m_manifest = {};
                }
            });
    connect(&m_offer, &SessionTransferOffer::incomingCancelled, this,
            [this](const QByteArray &id, auto) {
                if (!m_hasOffer || id != m_manifest.transferId) return;
                stop();
                emit cancelled();
            });
    connect(&m_session, &SessionNegotiationCoordinator::fileMessageReceived, this,
            [this](FileChannel channel, const QByteArray &message) {
                if (channel != FileChannel::Data || !m_active) return;
                QString error;
                if (!m_worker.enqueue(message, error)) fail(error);
            });
    connect(&m_session, &SessionNegotiationCoordinator::fileAvailabilityChanged, this, [this] {
        if (m_active && (!m_session.isFileChannelOpen(FileChannel::Metadata) ||
                         !m_session.isFileChannelOpen(FileChannel::Data)))
            fail(QStringLiteral("File session was interrupted."));
    });
    connect(&m_session, &SessionNegotiationCoordinator::stateChanged, this,
            [this](SessionNegotiationCoordinator::State state) {
                if (state == SessionNegotiationCoordinator::State::Idle ||
                    state == SessionNegotiationCoordinator::State::Ending) stop();
            });
    connect(&m_worker, &SessionFileReceiveWorker::receiptReady, this, &SessionFileIncoming::queueReceipt);
    connect(&m_worker, &SessionFileReceiveWorker::fileCommitted, this, &SessionFileIncoming::fileCommitted);
    connect(&m_worker, &SessionFileReceiveWorker::failed, this, &SessionFileIncoming::fail);
    connect(&m_worker, &SessionFileReceiveWorker::batchCompleted, this, [this] {
        if (!m_active) return;
        m_batchDone = true;
        m_session.setFileReceivingEnabled(false);
        flushReceipts();
    });
}

SessionFileIncoming::~SessionFileIncoming() { stop(); }

bool SessionFileIncoming::accept(const std::shared_ptr<ReceiveDirectory> &directory,
                                 quint32 windowBytes, QString &error) {
    if (!m_hasOffer || m_active || m_cancelling || !m_session.isSessionReady() || !directory) {
        error = QStringLiteral("File offer is not available for acceptance.");
        return false;
    }
    const auto context = m_session.context();
    if (m_trustedPeers.status(context.peerDeviceId, context.peerPublicKey) != TrustedPeerStore::Status::Matched) {
        error = QStringLiteral("Peer identity has not been verified for this device.");
        return false;
    }
    if (!m_worker.start(m_manifest, directory, error))
        return false;
    if (!m_session.setFileReceivingEnabled(true)) {
        m_worker.stop();
        error = QStringLiteral("File receive channel is unavailable.");
        return false;
    }
    if (!m_offer.acceptIncoming(directory, windowBytes, error)) {
        m_session.setFileReceivingEnabled(false);
        m_worker.stop();
        return false;
    }
    m_active = true;
    m_batchDone = false;
    error.clear();
    return true;
}

bool SessionFileIncoming::cancel(QString &error) {
    if (!m_hasOffer) {
        error = QStringLiteral("No incoming transfer is active.");
        return false;
    }
    if (!m_offer.cancelIncoming(easydesk::client::transfer::TransferCancelReason::ReceiverCancelled, error))
        return false;
    if (m_hasOffer) {
        m_cancelling = true;
        m_active = false;
        m_retry.stop();
        m_timeout.stop();
        m_receipts.clear();
        m_receiptBytes = 0;
        m_session.setFileReceivingEnabled(false);
        m_worker.stop();
    }
    return true;
}

void SessionFileIncoming::stop() {
    const QByteArray transferId = m_manifest.transferId;
    m_active = false;
    m_cancelling = false;
    m_hasOffer = false;
    m_batchDone = false;
    m_retry.stop();
    m_timeout.stop();
    m_receipts.clear();
    m_receiptBytes = 0;
    m_session.setFileReceivingEnabled(false);
    m_worker.stop();
    m_offer.finishIncoming(transferId);
    m_manifest = {};
}

void SessionFileIncoming::queueReceipt(const QByteArray &receipt) {
    if (!m_active) return;
    if (receipt.isEmpty() || receipt.size() > kMaximumReceiptBytes - m_receiptBytes) {
        fail(QStringLiteral("File receipt queue is full."));
        return;
    }
    m_receiptBytes += receipt.size();
    m_receipts.enqueue(receipt);
    if (!m_timeout.isActive()) m_timeout.start(kReceiptTimeoutMilliseconds);
    flushReceipts();
}

void SessionFileIncoming::flushReceipts() {
    if (!m_active) return;
    while (!m_receipts.isEmpty()) {
        if (!m_session.isSessionReady()) { fail(QStringLiteral("File session was interrupted.")); return; }
        const auto result = m_session.sendFileMessage(FileChannel::Metadata, m_receipts.head());
        if (result == SessionFileTransport::SendResult::WouldBlock) {
            m_retry.start(kRetryMilliseconds);
            return;
        }
        if (result != SessionFileTransport::SendResult::Sent) {
            fail(QStringLiteral("File receipt could not be sent."));
            return;
        }
        m_receiptBytes -= m_receipts.dequeue().size();
    }
    m_timeout.stop();
    m_retry.stop();
    if (m_batchDone) {
        stop();
        emit batchCompleted();
    }
}

void SessionFileIncoming::fail(const QString &error) {
    if (!m_active && !m_cancelling) return;
    stop();
    emit failed(error);
}
