#include "sessions/session_invitation_coordinator.h"

#include "data/settings/identity/device_profile_store.h"
#include "identity/windows_identity_store.h"
#include "media/webrtc/media_runtime_library.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"
#include "network/signaling/protocol/session/session_accept_codec.h"
#include "network/signaling/protocol/session/session_decision_codec.h"
#include "network/signaling/protocol/session/session_decision_decoder.h"
#include "network/signaling/protocol/session/session_delivery_codec.h"
#include "security/secure_random.h"

#include <QDateTime>

#include <utility>

namespace {

constexpr int kConsentTimeoutMilliseconds = 45000;
constexpr int kDeliveryTimeoutMilliseconds = 15000;
constexpr qint64 kDecisionLifetimeSeconds = 45;
constexpr qint64 kMaximumDecisionLifetimeSeconds = 60;
constexpr qint64 kActiveSessionLifetimeSeconds = 4 * 60 * 60;

bool containsCapability(
    const QVector<easydesk::client::signaling::SessionCapabilityGrant> &grants,
    easydesk::client::signaling::SessionCapability capability) {
    for (const auto &grant : grants) {
        if (grant.capability == capability)
            return true;
    }
    return false;
}

quint32 epochFromBytes(const QByteArray &bytes) {
    quint32 value = 0;
    for (const char byte : bytes)
        value = (value << 8U) | static_cast<unsigned char>(byte);
    return value == 0 ? 1 : value;
}

}  // namespace

SessionInvitationCoordinator::SessionInvitationCoordinator(
    DeviceProfileStore &profile,
    WindowsIdentityStore &identity,
    MediaRuntimeLibrary &crypto,
    SignalingClient &signaling,
    QObject *parent)
    : QObject(parent),
      m_profile(profile),
      m_identity(identity),
      m_crypto(crypto),
      m_signaling(signaling) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionInvitationCoordinator::handleTimeout);
    connect(&m_signaling, &SignalingClient::messageReceived,
            this, &SessionInvitationCoordinator::handleMessage);
    connect(&m_signaling, &SignalingClient::errorOccurred,
            this, &SessionInvitationCoordinator::handleSignalingError);
    connect(&m_signaling, &SignalingClient::stateChanged, this,
            [this](SignalingClient::State state) {
                if (m_state != State::Idle && state != SignalingClient::State::Registered) {
                    failInvitation(QStringLiteral("E_CONNECTION_STATE"),
                                   QStringLiteral("Signaling connection was interrupted."));
                }
            });
}

SessionInvitationCoordinator::State SessionInvitationCoordinator::state() const {
    return m_state;
}

IncomingSessionInvitation SessionInvitationCoordinator::invitation() const {
    return m_invitation;
}

QByteArray SessionInvitationCoordinator::requestId() const {
    return m_request.fields.requestId;
}

QByteArray SessionInvitationCoordinator::sessionId() const {
    return m_request.fields.sessionId;
}

QByteArray SessionInvitationCoordinator::requesterPublicKey() const {
    return m_request.fields.requesterPublicKey;
}

QByteArray SessionInvitationCoordinator::signedRequestPayload() const {
    return m_request.signedPayload;
}

quint32 SessionInvitationCoordinator::sessionEpoch() const {
    return m_sessionEpoch;
}

SessionMode SessionInvitationCoordinator::grantedMode() const {
    return m_grantedMode;
}

SessionNegotiationContext SessionInvitationCoordinator::negotiationContext() const {
    return m_negotiation;
}

bool SessionInvitationCoordinator::accept(SessionMode grantedMode) {
    if (m_state != State::AwaitingConsent ||
        (grantedMode != SessionMode::ViewOnly && grantedMode != SessionMode::Control) ||
        (grantedMode == SessionMode::Control &&
         m_invitation.requestedMode != SessionMode::Control)) {
        return false;
    }

    QString error;
    const QByteArray nonce =
        easydesk::client::windows::generateSecureRandomBytes(32, error);
    const QByteArray epochBytes =
        easydesk::client::windows::generateSecureRandomBytes(4, error);
    if (nonce.isEmpty() || epochBytes.isEmpty()) {
        failInvitation(QStringLiteral("E_RANDOM"), error);
        return false;
    }

    using ProtocolCapability = easydesk::client::signaling::SessionCapability;
    using ProtocolGrant = easydesk::client::signaling::SessionCapabilityGrant;
    const auto &request = m_request.fields;
    const QByteArray controllerPublicKey =
        request.requesterRole == easydesk::client::signaling::SessionParticipantRole::Controller
            ? request.requesterPublicKey
            : request.targetPublicKey;
    QVector<ProtocolGrant> granted{
        {controllerPublicKey, ProtocolCapability::ViewScreen},
    };
    if (grantedMode == SessionMode::Control)
        granted.append({controllerPublicKey, ProtocolCapability::SendInput});

    m_sessionEpoch = epochFromBytes(epochBytes);
    easydesk::client::signaling::SessionAcceptFields fields;
    fields.request = request;
    fields.signedRequestPayload = m_request.signedPayload;
    fields.nonce = nonce;
    fields.grantedGrants = std::move(granted);
    fields.sessionEpoch = m_sessionEpoch;
    fields.permissionEpoch = 1;
    fields.expiresAt = QDateTime::currentSecsSinceEpoch() +
                       kActiveSessionLifetimeSeconds;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionAcceptSigningBytes(fields, error);
    if (signingBytes.isEmpty()) {
        failInvitation(QStringLiteral("E_PROTOCOL"), error);
        return false;
    }
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signature.isEmpty()) {
        failInvitation(QStringLiteral("E_SIGNATURE"), m_identity.errorString());
        return false;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionAcceptEnvelope(fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        failInvitation(QStringLiteral("E_SEND"),
                       error.isEmpty()
                           ? QStringLiteral("Session acceptance could not be sent.")
                           : error);
        return false;
    }

    m_grantedMode = grantedMode;
    m_negotiation.requestId = request.requestId;
    m_negotiation.sessionId = request.sessionId;
    m_negotiation.signedRequestPayload = m_request.signedPayload;
    m_negotiation.signedAcceptPayload = signingBytes;
    m_negotiation.acceptanceNonce = nonce;
    m_negotiation.peerPublicKey = request.requesterPublicKey;
    m_negotiation.localDeviceId = request.toDeviceId;
    m_negotiation.peerDeviceId = request.fromDeviceId;
    m_negotiation.sessionEpoch = fields.sessionEpoch;
    m_negotiation.permissionEpoch = fields.permissionEpoch;
    m_negotiation.expiresAt = fields.expiresAt;
    m_negotiation.role =
        request.requesterRole ==
                easydesk::client::signaling::SessionParticipantRole::Controller
            ? SessionRole::Controlled
            : SessionRole::Controller;
    m_negotiation.capabilities = capabilitiesForMode(grantedMode);
    m_negotiation.offerer = false;
    setState(State::AwaitingAcceptanceDelivery);
    m_timeout.start(kDeliveryTimeoutMilliseconds);
    return true;
}

bool SessionInvitationCoordinator::reject() {
    return sendRejection(easydesk::client::signaling::SessionRejectReason::Declined);
}

void SessionInvitationCoordinator::handleMessage(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        if (m_state != State::Idle)
            failInvitation(QStringLiteral("E_PROTOCOL"), error);
        return;
    }

    if (m_state == State::Idle && decoded.type == QStringLiteral("session.request")) {
        handleRequest(message);
    } else if (m_state == State::AwaitingConsent &&
               decoded.type == QStringLiteral("session.cancel")) {
        handleCancellation(message);
    } else if ((m_state == State::AwaitingAcceptanceDelivery ||
                m_state == State::AwaitingRejectionDelivery) &&
               decoded.type == QStringLiteral("session.delivered")) {
        handleDelivery(message);
    }
}

void SessionInvitationCoordinator::handleRequest(const QByteArray &message) {
    easydesk::client::signaling::DecodedSessionRequest request;
    QString error;
    if (!easydesk::client::signaling::decodeSessionRequestEnvelope(message, request, error)) {
        emit failed(QStringLiteral("E_PROTOCOL"), error);
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (request.fields.toDeviceId != m_profile.profile().deviceId ||
        request.fields.targetPublicKey != m_identity.publicKey() ||
        request.fields.expiresAt < now || request.fields.expiresAt > now + 60) {
        emit failed(QStringLiteral("E_INVALID_ROUTE"),
                    QStringLiteral("Incoming session request did not match this device."));
        return;
    }
    if (!m_crypto.verifyEd25519(request.fields.requesterPublicKey,
                               request.signedPayload,
                               request.signature)) {
        emit failed(QStringLiteral("E_SIGNATURE"), m_crypto.errorString());
        return;
    }
    if (!containsCapability(request.fields.requestedGrants,
                            easydesk::client::signaling::SessionCapability::ViewScreen)) {
        emit failed(QStringLiteral("E_UNSUPPORTED_CAPABILITY"),
                    QStringLiteral("Incoming request does not include screen viewing."));
        return;
    }

    m_request = std::move(request);
    m_invitation.fromDeviceId = m_request.fields.fromDeviceId;
    m_invitation.requestedMode =
        containsCapability(m_request.fields.requestedGrants,
                           easydesk::client::signaling::SessionCapability::SendInput)
            ? SessionMode::Control
            : SessionMode::ViewOnly;
    m_invitation.requesterIsController =
        m_request.fields.requesterRole ==
        easydesk::client::signaling::SessionParticipantRole::Controller;
    m_invitation.expiresAt = m_request.fields.expiresAt;
    setState(State::AwaitingConsent);
    m_timeout.start(kConsentTimeoutMilliseconds);
    emit invitationReceived(m_invitation);
}

void SessionInvitationCoordinator::handleCancellation(const QByteArray &message) {
    easydesk::client::signaling::DecodedSessionDecision cancellation;
    QString error;
    if (!easydesk::client::signaling::decodeSessionDecisionEnvelope(
            message,
            m_request.fields,
            m_request.signedPayload,
            cancellation,
            error) ||
        cancellation.kind !=
            easydesk::client::signaling::SessionDecisionKind::Cancel ||
        cancellation.sequence != 1) {
        failInvitation(QStringLiteral("E_PROTOCOL"),
                       error.isEmpty()
                           ? QStringLiteral("Session cancellation is invalid.")
                           : error);
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (cancellation.expiresAt < now ||
        cancellation.expiresAt > now + kMaximumDecisionLifetimeSeconds) {
        failInvitation(QStringLiteral("E_EXPIRED"),
                       QStringLiteral("Session cancellation lifetime is invalid."));
        return;
    }
    if (!m_crypto.verifyEd25519(m_request.fields.requesterPublicKey,
                                cancellation.signedPayload,
                                cancellation.signature)) {
        failInvitation(QStringLiteral("E_SIGNATURE"), m_crypto.errorString());
        return;
    }

    const quint8 reason = cancellation.reason;
    m_timeout.stop();
    clearInvitation();
    setState(State::Idle);
    emit invitationCancelled(reason);
}

void SessionInvitationCoordinator::handleDelivery(const QByteArray &message) {
    easydesk::client::signaling::SessionDeliveryMessage delivery;
    QString error;
    if (!easydesk::client::signaling::decodeSessionDelivery(message, delivery, error) ||
        delivery.requestId != m_request.fields.requestId ||
        delivery.sessionId != m_request.fields.sessionId || delivery.sequence != 1) {
        failInvitation(QStringLiteral("E_PROTOCOL"),
                       error.isEmpty()
                           ? QStringLiteral("Session decision receipt did not match the request.")
                           : error);
        return;
    }

    if (m_state == State::AwaitingAcceptanceDelivery) {
        if (delivery.messageType != QStringLiteral("session.accept") ||
            !delivery.hasSessionEpoch || delivery.sessionEpoch != m_sessionEpoch) {
            failInvitation(QStringLiteral("E_PROTOCOL"),
                           QStringLiteral("Session acceptance receipt is invalid."));
            return;
        }
        m_timeout.stop();
        setState(State::Accepted);
        emit sessionAccepted();
        return;
    }

    if (delivery.messageType != QStringLiteral("session.reject") ||
        delivery.hasSessionEpoch) {
        failInvitation(QStringLiteral("E_PROTOCOL"),
                       QStringLiteral("Session rejection receipt is invalid."));
        return;
    }
    m_timeout.stop();
    clearInvitation();
    setState(State::Idle);
    emit invitationRejected();
}

void SessionInvitationCoordinator::handleSignalingError(const QString &code,
                                                        const QString &message) {
    if (m_state != State::Idle)
        failInvitation(code, message);
}

void SessionInvitationCoordinator::handleTimeout() {
    if (m_state == State::AwaitingConsent) {
        (void)sendRejection(
            easydesk::client::signaling::SessionRejectReason::Unavailable);
    } else if (m_state == State::AwaitingAcceptanceDelivery ||
               m_state == State::AwaitingRejectionDelivery) {
        failInvitation(QStringLiteral("E_DELIVERY_TIMEOUT"),
                       QStringLiteral("Session decision delivery timed out."));
    }
}

bool SessionInvitationCoordinator::sendRejection(
    easydesk::client::signaling::SessionRejectReason reason) {
    if (m_state != State::AwaitingConsent)
        return false;
    QString error;
    easydesk::client::signaling::SessionDecisionFields fields;
    fields.request = m_request.fields;
    fields.signedRequestPayload = m_request.signedPayload;
    fields.sequence = 1;
    fields.expiresAt = QDateTime::currentSecsSinceEpoch() +
                       kDecisionLifetimeSeconds;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionRejectSigningBytes(fields, reason, error);
    if (signingBytes.isEmpty()) {
        failInvitation(QStringLiteral("E_PROTOCOL"), error);
        return false;
    }
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signature.isEmpty()) {
        failInvitation(QStringLiteral("E_SIGNATURE"), m_identity.errorString());
        return false;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionRejectEnvelope(
            fields, reason, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        failInvitation(QStringLiteral("E_SEND"),
                       error.isEmpty()
                           ? QStringLiteral("Session rejection could not be sent.")
                           : error);
        return false;
    }
    setState(State::AwaitingRejectionDelivery);
    m_timeout.start(kDeliveryTimeoutMilliseconds);
    return true;
}

void SessionInvitationCoordinator::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SessionInvitationCoordinator::failInvitation(const QString &code,
                                                  const QString &message) {
    m_timeout.stop();
    clearInvitation();
    setState(State::Idle);
    emit failed(code, message);
}

void SessionInvitationCoordinator::clearInvitation() {
    m_request = {};
    m_invitation = {};
    m_sessionEpoch = 0;
    m_grantedMode = SessionMode::ViewOnly;
    m_negotiation = {};
}
