#include "sessions/files/session_transfer_offer.h"

#include "sessions/negotiation/session_negotiation_coordinator.h"
#include "files/receive_directory.h"

namespace {
constexpr int kRetryMilliseconds = 25;
constexpr int kSendTimeoutMilliseconds = 30000;
constexpr int kReceiveTimeoutMilliseconds = 15000;
constexpr int kDecisionTimeoutMilliseconds = 60000;
}

SessionTransferOffer::SessionTransferOffer(SessionNegotiationCoordinator &session, QObject *parent)
    : QObject(parent), m_session(session) {
    m_sendTimer.setSingleShot(true);
    m_sendTimeout.setSingleShot(true);
    m_receiveTimer.setSingleShot(true);
    m_decisionTimer.setSingleShot(true);
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
    connect(&m_session, &SessionNegotiationCoordinator::fileMessageReceived, this,
            [this](FileChannel channel, const QByteArray &message) {
                if (channel == FileChannel::Metadata) {
                    if (easydesk::client::transfer::isTransferDecision(message))
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

bool SessionTransferOffer::respond(easydesk::client::transfer::OfferDecision decision,
                                   const std::shared_ptr<ReceiveDirectory> &directory,
                                   quint32 windowBytes, QString &error) {
    using namespace easydesk::client::transfer;
    if (!m_incomingPending || m_incomingResolved || !m_session.context().localMayReceiveFiles ||
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
    error.clear();
    emit decisionSent(decision);
    return true;
}

void SessionTransferOffer::reset() {
    m_sendTimer.stop();
    m_sendTimeout.stop();
    m_receiveTimer.stop();
    m_decisionTimer.stop();
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
    m_decisionTimer.start(kDecisionTimeoutMilliseconds);
    emit offerSent(m_outgoingId);
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
    emit offerReceived(manifest);
}

void SessionTransferOffer::receiveDecision(const QByteArray &message) {
    using namespace easydesk::client::transfer;
    TransferDecision response;
    QString error;
    if (!m_outgoingPending || !m_outgoingSent || m_outgoingResolved ||
        !m_session.context().peerMayReceiveFiles || !m_session.isSessionReady() ||
        !decodeTransferDecision(message, response, error) || response.transferId != m_outgoingId ||
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

void SessionTransferOffer::abort(const QString &error) {
    reset();
    emit failed(error);
}
