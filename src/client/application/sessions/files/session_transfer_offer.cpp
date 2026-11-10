#include "sessions/files/session_transfer_offer.h"

#include "sessions/negotiation/session_negotiation_coordinator.h"
#include "files/receive_directory.h"
#include "network/files/protocol/file_receipt_codec.h"

namespace {
constexpr int kRetryMilliseconds = 25;
constexpr int kSendTimeoutMilliseconds = 30000;
constexpr int kReceiveTimeoutMilliseconds = 15000;
constexpr int kDecisionTimeoutMilliseconds = 60000;
constexpr int kIncomingDecisionTimeoutMilliseconds = 45000;
constexpr int kCancelTimeoutMilliseconds = 10000;
constexpr int kMaximumCancelledOffers = 128;

bool senderReason(easydesk::client::transfer::TransferCancelReason reason) {
    using easydesk::client::transfer::TransferCancelReason;
    return reason == TransferCancelReason::SenderCancelled || reason == TransferCancelReason::SenderFailed;
}

bool receiverReason(easydesk::client::transfer::TransferCancelReason reason) {
    using easydesk::client::transfer::TransferCancelReason;
    return reason == TransferCancelReason::ReceiverCancelled || reason == TransferCancelReason::ReceiverFailed;
}
}

SessionTransferOffer::SessionTransferOffer(SessionNegotiationCoordinator &session, QObject *parent)
    : QObject(parent), m_session(session) {
    m_sendTimer.setSingleShot(true);
    m_sendTimeout.setSingleShot(true);
    m_receiveTimer.setSingleShot(true);
    m_decisionTimer.setSingleShot(true);
    m_incomingDecisionTimer.setSingleShot(true);
    m_outgoingCancelRetry.setSingleShot(true);
    m_outgoingCancelTimeout.setSingleShot(true);
    m_incomingCancelRetry.setSingleShot(true);
    m_incomingCancelTimeout.setSingleShot(true);
    connect(&m_outgoingCancelRetry, &QTimer::timeout, this, &SessionTransferOffer::sendOutgoingCancellation);
    connect(&m_incomingCancelRetry, &QTimer::timeout, this, &SessionTransferOffer::sendIncomingCancellation);
    connect(&m_outgoingCancelTimeout, &QTimer::timeout, this, [this] {
        abort(QStringLiteral("Transfer cancellation timed out."));
    });
    connect(&m_incomingCancelTimeout, &QTimer::timeout, this, [this] {
        abort(QStringLiteral("Transfer cancellation timed out."));
    });
    connect(&m_sendTimer, &QTimer::timeout, this, &SessionTransferOffer::sendNext);
    connect(&m_sendTimeout, &QTimer::timeout, this, [this] {
        abort(QStringLiteral("Transfer offer send timed out."));
    });
    connect(&m_receiveTimer, &QTimer::timeout, this, [this] {
        abort(QStringLiteral("Transfer offer timed out."));
    });
    connect(&m_decisionTimer, &QTimer::timeout, this, [this] {
        abort(QStringLiteral("Transfer decision timed out."));
    });
    connect(&m_incomingDecisionTimer, &QTimer::timeout, this, [this] {
        if (!m_incomingPending || m_incomingResolved)
            return;
        const QByteArray expiredId = m_incomingId;
        QString error;
        if (!declineIncoming(error)) {
            abort(error);
            return;
        }
        emit incomingOfferExpired(expiredId);
    });
    connect(&m_session, &SessionNegotiationCoordinator::fileMessageReceived, this,
            [this](FileChannel channel, const QByteArray &message) {
                if (channel == FileChannel::Metadata) {
                    if (easydesk::client::transfer::isFileReceipt(message))
                        return;
                    if (easydesk::client::transfer::isTransferCancellation(message))
                        receiveCancellation(message);
                    else if (easydesk::client::transfer::isTransferDecision(message))
                        receiveDecision(message);
                    else
                        receive(message);
                }
            });
    connect(&m_session, &SessionNegotiationCoordinator::fileAvailabilityChanged, this, [this] {
        if ((m_outgoingPending || m_incomingPending || m_receiving) &&
            (!m_session.isFileChannelOpen(FileChannel::Metadata) ||
             !m_session.isFileChannelOpen(FileChannel::Data)))
            abort(QStringLiteral("File session was interrupted."));
    });
    connect(&m_session, &SessionNegotiationCoordinator::stateChanged, this,
            [this](SessionNegotiationCoordinator::State state) {
                if (state == SessionNegotiationCoordinator::State::Idle ||
                    state == SessionNegotiationCoordinator::State::Ending)
                    reset();
            });
}

void SessionTransferOffer::finishIncoming(const QByteArray &transferId) {
    if (!m_incomingPending || !m_incomingResolved || transferId != m_incomingId)
        return;
    clearIncoming();
}

void SessionTransferOffer::clearIncoming() {
    m_incomingDecisionTimer.stop();
    m_incomingCancelRetry.stop();
    m_incomingCancelTimeout.stop();
    m_incomingCancelRequested = false;
    m_incomingPending = false;
    m_incomingResolved = false;
    m_incomingId.clear();
    m_incomingDigest.clear();
    m_acceptedDirectory.reset();
}

void SessionTransferOffer::finishOutgoing(const QByteArray &transferId) {
    if (!m_outgoingPending || transferId != m_outgoingId)
        return;
    m_sendTimer.stop();
    m_sendTimeout.stop();
    m_decisionTimer.stop();
    m_outgoingCancelRetry.stop();
    m_outgoingCancelTimeout.stop();
    m_outgoingCancelRequested = false;
    m_frames.clear();
    m_outgoingId.clear();
    m_outgoingDigest.clear();
    m_nextFrame = 0;
    m_outgoingPending = false;
    m_outgoingSent = false;
    m_outgoingResolved = false;
}

bool SessionTransferOffer::sendOffer(const easydesk::client::transfer::TransferManifest &manifest,
                                     QString &error) {
    if (m_outgoingPending || !m_session.context().peerMayReceiveFiles || !m_session.isSessionReady() ||
        !m_session.isFileChannelOpen(FileChannel::Metadata) ||
        !m_session.isFileChannelOpen(FileChannel::Data)) {
        error = QStringLiteral("File session is unavailable or already has an offer.");
        return false;
    }
    if (!easydesk::client::transfer::encodeTransferOffer(manifest, m_frames, error))
        return false;
    m_outgoingId = manifest.transferId;
    m_outgoingDigest = easydesk::client::transfer::manifestDigest(
        manifest, easydesk::client::transfer::kDefaultMaximumFileBytes, error);
    if (m_outgoingDigest.size() != 32) {
        m_frames.clear();
        m_outgoingId.clear();
        return false;
    }
    m_outgoingPending = true;
    m_nextFrame = 0;
    m_sendTimer.start(0);
    m_sendTimeout.start(kSendTimeoutMilliseconds);
    error.clear();
    return true;
}

bool SessionTransferOffer::acceptIncoming(const std::shared_ptr<ReceiveDirectory> &directory,
                                          quint32 windowBytes, QString &error) {
    return respond(easydesk::client::transfer::OfferDecision::Accept, directory, windowBytes, error);
}

bool SessionTransferOffer::declineIncoming(QString &error) {
    return respond(easydesk::client::transfer::OfferDecision::Decline, {}, 0, error);
}

bool SessionTransferOffer::cancelOutgoing(easydesk::client::transfer::TransferCancelReason reason,
                                          QString &error) {
    using namespace easydesk::client::transfer;
    if (!senderReason(reason) || !m_outgoingPending || !m_session.isSessionReady()) {
        error = QStringLiteral("Outgoing transfer cannot be cancelled yet.");
        return false;
    }
    if (m_outgoingCancelRequested) {
        error.clear();
        return true;
    }
    if (!m_outgoingSent && m_nextFrame == 0) {
        const QByteArray id = m_outgoingId;
        finishOutgoing(id);
        error.clear();
        emit outgoingCancelled(id, reason);
        return true;
    }
    m_outgoingCancelRequested = true;
    m_outgoingCancelReason = reason;
    m_decisionTimer.stop();
    m_outgoingCancelTimeout.start(kCancelTimeoutMilliseconds);
    if (m_outgoingSent)
        m_outgoingCancelRetry.start(0);
    else
        m_sendTimer.start(0);
    error.clear();
    return true;
}

bool SessionTransferOffer::cancelIncoming(easydesk::client::transfer::TransferCancelReason reason,
                                          QString &error) {
    using namespace easydesk::client::transfer;
    if (!receiverReason(reason) || !m_incomingPending || !m_session.isSessionReady()) {
        error = QStringLiteral("Incoming transfer cannot be cancelled.");
        return false;
    }
    if (!m_incomingCancelRequested) {
        m_incomingCancelRequested = true;
        m_incomingCancelReason = reason;
        m_incomingDecisionTimer.stop();
        m_incomingCancelTimeout.start(kCancelTimeoutMilliseconds);
        m_incomingCancelRetry.start(0);
    }
    error.clear();
    return true;
}

bool SessionTransferOffer::respond(easydesk::client::transfer::OfferDecision decision,
                                   const std::shared_ptr<ReceiveDirectory> &directory,
                                   quint32 windowBytes, QString &error) {
    using namespace easydesk::client::transfer;
    if (!m_incomingPending || m_incomingResolved || m_incomingCancelRequested || !m_session.context().localMayReceiveFiles ||
        !m_session.isSessionReady() || !m_session.isFileChannelOpen(FileChannel::Data)) {
        error = QStringLiteral("Transfer offer is not awaiting a decision.");
        return false;
    }
    TransferDecision response;
    response.transferId = m_incomingId;
    response.manifestDigest = m_incomingDigest;
    response.decision = decision;
    response.rootId = directory ? directory->rootId() : QByteArray();
    response.windowBytes = windowBytes;
    QByteArray message;
    if (!encodeTransferDecision(response, message, error))
        return false;
    const auto result = m_session.sendFileMessage(FileChannel::Metadata, message);
    if (result != SessionFileTransport::SendResult::Sent) {
        error = result == SessionFileTransport::SendResult::WouldBlock
                    ? QStringLiteral("Transfer decision channel is busy.")
                    : QStringLiteral("Transfer decision could not be sent.");
        return false;
    }
    if (decision == OfferDecision::Accept) {
        m_acceptedDirectory = directory;
        m_incomingResolved = true;
    } else {
        m_incomingPending = false;
        m_incomingId.clear();
        m_incomingDigest.clear();
    }
    m_incomingDecisionTimer.stop();
    error.clear();
    emit decisionSent(decision);
    return true;
}

void SessionTransferOffer::reset() {
    m_sendTimer.stop();
    m_sendTimeout.stop();
    m_receiveTimer.stop();
    m_decisionTimer.stop();
    m_incomingDecisionTimer.stop();
    m_outgoingCancelRetry.stop();
    m_outgoingCancelTimeout.stop();
    m_incomingCancelRetry.stop();
    m_incomingCancelTimeout.stop();
    m_decoder.reset();
    m_frames.clear();
    m_outgoingId.clear();
    m_outgoingDigest.clear();
    m_incomingId.clear();
    m_incomingDigest.clear();
    m_acceptedDirectory.reset();
    m_nextFrame = 0;
    m_outgoingPending = false;
    m_outgoingSent = false;
    m_outgoingResolved = false;
    m_incomingPending = false;
    m_incomingResolved = false;
    m_receiving = false;
    m_outgoingCancelRequested = false;
    m_incomingCancelRequested = false;
    m_cancelledOutgoing.clear();
    m_cancelledOrder.clear();
}

void SessionTransferOffer::sendNext() {
    if (!m_outgoingPending || m_nextFrame >= m_frames.size())
        return;
    if (!m_session.isSessionReady()) {
        abort(QStringLiteral("File session was interrupted."));
        return;
    }
    const auto result = m_session.sendFileMessage(FileChannel::Metadata, m_frames.at(m_nextFrame));
    if (result == SessionFileTransport::SendResult::WouldBlock) {
        m_sendTimer.start(kRetryMilliseconds);
        return;
    }
    if (result != SessionFileTransport::SendResult::Sent) {
        abort(QStringLiteral("Transfer offer could not be sent."));
        return;
    }
    if (++m_nextFrame < m_frames.size()) {
        m_sendTimer.start(0);
        return;
    }
    m_frames.clear();
    m_sendTimeout.stop();
    m_outgoingSent = true;
    if (m_outgoingCancelRequested)
        m_outgoingCancelRetry.start(0);
    else
        m_decisionTimer.start(kDecisionTimeoutMilliseconds);
    const QByteArray sentId = m_outgoingId;
    emit offerSent(sentId);
}

void SessionTransferOffer::receive(const QByteArray &message) {
    if (m_incomingPending || !m_session.context().localMayReceiveFiles || !m_session.isSessionReady() ||
        !m_session.isFileChannelOpen(FileChannel::Data)) {
        abort(QStringLiteral("Transfer offer is not allowed in the current state."));
        return;
    }
    easydesk::client::transfer::TransferManifest manifest;
    QString error;
    const auto result = m_decoder.receive(message, manifest, error);
    if (result == easydesk::client::transfer::TransferOfferDecoder::Result::Invalid) {
        abort(error);
        return;
    }
    if (result == easydesk::client::transfer::TransferOfferDecoder::Result::Pending) {
        m_receiving = true;
        m_receiveTimer.start(kReceiveTimeoutMilliseconds);
        return;
    }
    m_receiveTimer.stop();
    m_receiving = false;
    m_incomingPending = true;
    m_incomingId = manifest.transferId;
    m_incomingDigest = easydesk::client::transfer::manifestDigest(
        manifest, easydesk::client::transfer::kDefaultMaximumFileBytes, error);
    if (m_incomingDigest.size() != 32) {
        abort(QStringLiteral("Transfer offer digest could not be calculated."));
        return;
    }
    m_incomingDecisionTimer.start(kIncomingDecisionTimeoutMilliseconds);
    emit offerReceived(manifest);
}

void SessionTransferOffer::receiveDecision(const QByteArray &message) {
    using namespace easydesk::client::transfer;
    TransferDecision response;
    QString error;
    if (!decodeTransferDecision(message, response, error)) {
        abort(QStringLiteral("Transfer decision is invalid."));
        return;
    }
    if (m_cancelledOutgoing.value(response.transferId) == response.manifestDigest)
        return;
    if (m_outgoingCancelRequested && response.transferId == m_outgoingId &&
        response.manifestDigest == m_outgoingDigest)
        return;
    if (!m_outgoingPending || !m_outgoingSent || m_outgoingResolved ||
        !m_session.context().peerMayReceiveFiles || !m_session.isSessionReady() ||
        response.transferId != m_outgoingId ||
        response.manifestDigest != m_outgoingDigest) {
        abort(QStringLiteral("Transfer decision does not match the pending offer."));
        return;
    }
    m_decisionTimer.stop();
    if (response.decision == OfferDecision::Accept) {
        m_outgoingResolved = true;
    } else {
        m_outgoingPending = false;
        m_outgoingSent = false;
        m_outgoingId.clear();
        m_outgoingDigest.clear();
    }
    emit decisionReceived(response);
}

void SessionTransferOffer::sendOutgoingCancellation() {
    using namespace easydesk::client::transfer;
    if (!m_outgoingCancelRequested || !m_outgoingPending || !m_outgoingSent) return;
    const QByteArray id = m_outgoingId;
    const QByteArray digest = m_outgoingDigest;
    const auto reason = m_outgoingCancelReason;
    QString error;
    QByteArray message;
    if (!encodeTransferCancellation({id, digest, reason}, message, error)) { abort(error); return; }
    const auto result = m_session.sendFileMessage(FileChannel::Metadata, message);
    if (result == SessionFileTransport::SendResult::WouldBlock) {
        m_outgoingCancelRetry.start(kRetryMilliseconds);
        return;
    }
    if (result != SessionFileTransport::SendResult::Sent) {
        abort(QStringLiteral("Transfer cancellation could not be sent."));
        return;
    }
    rememberCancelledOutgoing(id, digest);
    finishOutgoing(id);
    emit outgoingCancelled(id, reason);
}

void SessionTransferOffer::sendIncomingCancellation() {
    using namespace easydesk::client::transfer;
    if (!m_incomingCancelRequested || !m_incomingPending) return;
    const QByteArray id = m_incomingId;
    const auto reason = m_incomingCancelReason;
    QString error;
    QByteArray message;
    if (!encodeTransferCancellation({id, m_incomingDigest, reason}, message, error)) { abort(error); return; }
    const auto result = m_session.sendFileMessage(FileChannel::Metadata, message);
    if (result == SessionFileTransport::SendResult::WouldBlock) {
        m_incomingCancelRetry.start(kRetryMilliseconds);
        return;
    }
    if (result != SessionFileTransport::SendResult::Sent) {
        abort(QStringLiteral("Transfer cancellation could not be sent."));
        return;
    }
    clearIncoming();
    emit incomingCancelled(id, reason);
}

void SessionTransferOffer::rememberCancelledOutgoing(const QByteArray &id, const QByteArray &digest) {
    if (!m_cancelledOutgoing.contains(id)) {
        if (m_cancelledOrder.size() >= kMaximumCancelledOffers)
            m_cancelledOutgoing.remove(m_cancelledOrder.dequeue());
        m_cancelledOrder.enqueue(id);
    }
    m_cancelledOutgoing.insert(id, digest);
}

void SessionTransferOffer::receiveCancellation(const QByteArray &message) {
    using namespace easydesk::client::transfer;
    TransferCancellation cancel;
    QString error;
    if (!decodeTransferCancellation(message, cancel, error)) {
        abort(error);
        return;
    }
    if (senderReason(cancel.reason) && m_incomingPending && cancel.transferId == m_incomingId &&
        cancel.manifestDigest == m_incomingDigest) {
        clearIncoming();
        emit incomingCancelled(cancel.transferId, cancel.reason);
    } else if (receiverReason(cancel.reason) && m_outgoingPending && cancel.transferId == m_outgoingId &&
               cancel.manifestDigest == m_outgoingDigest) {
        if (!m_outgoingResolved) rememberCancelledOutgoing(m_outgoingId, m_outgoingDigest);
        finishOutgoing(cancel.transferId);
        emit outgoingCancelled(cancel.transferId, cancel.reason);
    }
}

void SessionTransferOffer::abort(const QString &error) {
    reset();
    emit failed(error);
}
