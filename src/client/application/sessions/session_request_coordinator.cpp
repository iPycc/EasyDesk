#include "sessions/session_request_coordinator.h"

#include "data/settings/identity/device_profile_store.h"
#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"
#include "network/signaling/protocol/session/session_delivery_codec.h"
#include "network/signaling/protocol/session/session_request_codec.h"
#include "security/secure_random.h"

#include <QDateTime>

namespace {

constexpr int kResolveTimeoutMilliseconds = 15000;
constexpr int kDeliveryTimeoutMilliseconds = 15000;
constexpr int kDecisionTimeoutMilliseconds = 55000;
constexpr qint64 kRequestLifetimeSeconds = 45;

bool containsDigits(const QString &value, int length) {
    if (value.size() != length)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

}  // namespace

SessionRequestCoordinator::SessionRequestCoordinator(DeviceProfileStore &profile,
                                                     WindowsIdentityStore &identity,
                                                     SignalingClient &signaling,
                                                     QObject *parent)
    : QObject(parent),
      m_profile(profile),
      m_identity(identity),
      m_signaling(signaling) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionRequestCoordinator::handleTimeout);
    connect(&m_signaling, &SignalingClient::messageReceived,
            this, &SessionRequestCoordinator::handleMessage);
    connect(&m_signaling, &SignalingClient::errorOccurred,
            this, &SessionRequestCoordinator::handleSignalingError);
    connect(&m_signaling, &SignalingClient::stateChanged, this,
            [this](SignalingClient::State state) {
                if (m_state != State::Idle && state != SignalingClient::State::Registered) {
                    failRequest(QStringLiteral("E_CONNECTION_STATE"),
                                QStringLiteral("Signaling connection was interrupted."));
                }
            });
}

SessionRequestCoordinator::State SessionRequestCoordinator::state() const {
    return m_state;
}

bool SessionRequestCoordinator::isAvailable() const {
    return m_state == State::Idle && m_signaling.isRegistered() && m_identity.isReady() &&
           !m_profile.profile().deviceId.isEmpty();
}

QString SessionRequestCoordinator::targetDeviceId() const {
    return m_targetDeviceId;
}

QByteArray SessionRequestCoordinator::targetPublicKey() const {
    return m_targetPublicKey;
}

QByteArray SessionRequestCoordinator::requestId() const {
    return m_requestId;
}

QByteArray SessionRequestCoordinator::sessionId() const {
    return m_sessionId;
}

QByteArray SessionRequestCoordinator::signedRequestPayload() const {
    return m_signedRequestPayload;
}

SessionMode SessionRequestCoordinator::mode() const {
    return m_mode;
}

bool SessionRequestCoordinator::requestSession(const QString &targetDeviceId,
                                               const QString &assistanceCode,
                                               SessionMode mode) {
    if (!isAvailable() || !containsDigits(targetDeviceId, 9) ||
        !containsDigits(assistanceCode, 8) ||
        targetDeviceId == m_profile.profile().deviceId ||
        (mode != SessionMode::ViewOnly && mode != SessionMode::Control)) {
        return false;
    }

    QString error;
    const QByteArray message = easydesk::client::signaling::encodeAssistanceResolve(
        assistanceCode, error);
    if (message.isEmpty() || !m_signaling.sendRegisteredMessage(message)) {
        emit failed(QStringLiteral("E_SEND"),
                    error.isEmpty()
                        ? QStringLiteral("Assistance identity request could not be sent.")
                        : error);
        return false;
    }

    clearRequest();
    m_targetDeviceId = targetDeviceId;
    m_mode = mode;
    setState(State::ResolvingIdentity);
    m_timeout.start(kResolveTimeoutMilliseconds);
    return true;
}

void SessionRequestCoordinator::handleMessage(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        if (m_state != State::Idle)
            failRequest(QStringLiteral("E_PROTOCOL"), error);
        return;
    }

    if (m_state == State::ResolvingIdentity &&
        decoded.kind == easydesk::client::signaling::BootstrapMessage::Kind::AssistanceResolved) {
        handleResolvedIdentity(message);
    } else if (m_state == State::AwaitingDelivery &&
               decoded.type == QStringLiteral("session.delivered")) {
        handleDelivery(message);
    }
}

void SessionRequestCoordinator::handleResolvedIdentity(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error) ||
        decoded.resolved.deviceId != m_targetDeviceId ||
        decoded.resolved.publicKey == m_identity.publicKey()) {
        failRequest(QStringLiteral("E_IDENTITY_MISMATCH"),
                    error.isEmpty()
                        ? QStringLiteral("Resolved assistance identity did not match the target.")
                        : error);
        return;
    }

    m_targetPublicKey = decoded.resolved.publicKey;
    m_requestId = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (m_requestId.isEmpty()) {
        failRequest(QStringLiteral("E_RANDOM"), error);
        return;
    }
    m_sessionId = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (m_sessionId.isEmpty()) {
        failRequest(QStringLiteral("E_RANDOM"), error);
        return;
    }
    const QByteArray nonce =
        easydesk::client::windows::generateSecureRandomBytes(32, error);
    if (nonce.isEmpty()) {
        failRequest(QStringLiteral("E_RANDOM"), error);
        return;
    }

    easydesk::client::signaling::SessionRequestFields fields;
    fields.requestId = m_requestId;
    fields.sessionId = m_sessionId;
    fields.requesterPublicKey = m_identity.publicKey();
    fields.targetPublicKey = m_targetPublicKey;
    fields.requesterRole =
        easydesk::client::signaling::SessionParticipantRole::Controller;
    fields.nonce = nonce;
    fields.requestedGrants.append(
        {fields.requesterPublicKey,
         easydesk::client::signaling::SessionCapability::ViewScreen});
    if (m_mode == SessionMode::Control) {
        fields.requestedGrants.append(
            {fields.requesterPublicKey,
             easydesk::client::signaling::SessionCapability::SendInput});
    }
    fields.expiresAt = QDateTime::currentSecsSinceEpoch() + kRequestLifetimeSeconds;
    fields.fromDeviceId = m_profile.profile().deviceId;
    fields.toDeviceId = m_targetDeviceId;

    m_signedRequestPayload =
        easydesk::client::signaling::sessionRequestSigningBytes(fields, error);
    if (m_signedRequestPayload.isEmpty()) {
        failRequest(QStringLiteral("E_PROTOCOL"), error);
        return;
    }
    const QByteArray signature = m_identity.sign(m_signedRequestPayload);
    if (signature.isEmpty()) {
        failRequest(QStringLiteral("E_SIGNATURE"), m_identity.errorString());
        return;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionRequestEnvelope(fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        failRequest(QStringLiteral("E_SEND"),
                    error.isEmpty()
                        ? QStringLiteral("Session request could not be sent.")
                        : error);
        return;
    }

    setState(State::AwaitingDelivery);
    m_timeout.start(kDeliveryTimeoutMilliseconds);
}

void SessionRequestCoordinator::handleDelivery(const QByteArray &message) {
    easydesk::client::signaling::SessionDeliveryMessage delivery;
    QString error;
    if (!easydesk::client::signaling::decodeSessionDelivery(message, delivery, error) ||
        delivery.messageType != QStringLiteral("session.request") ||
        delivery.requestId != m_requestId || delivery.sessionId != m_sessionId ||
        delivery.sequence != 1 || delivery.hasSessionEpoch) {
        failRequest(QStringLiteral("E_PROTOCOL"),
                    error.isEmpty()
                        ? QStringLiteral("Session delivery receipt did not match the request.")
                        : error);
        return;
    }

    setState(State::AwaitingDecision);
    m_timeout.start(kDecisionTimeoutMilliseconds);
    emit requestDelivered();
}

void SessionRequestCoordinator::handleSignalingError(const QString &code,
                                                     const QString &message) {
    if (m_state != State::Idle)
        failRequest(code, message);
}

void SessionRequestCoordinator::handleTimeout() {
    switch (m_state) {
    case State::ResolvingIdentity:
        failRequest(QStringLiteral("E_RESOLVE_TIMEOUT"),
                    QStringLiteral("Assistance identity lookup timed out."));
        break;
    case State::AwaitingDelivery:
        failRequest(QStringLiteral("E_DELIVERY_TIMEOUT"),
                    QStringLiteral("Session request delivery timed out."));
        break;
    case State::AwaitingDecision:
        failRequest(QStringLiteral("E_DECISION_TIMEOUT"),
                    QStringLiteral("The remote device did not respond in time."));
        break;
    case State::Idle:
        break;
    }
}

void SessionRequestCoordinator::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SessionRequestCoordinator::failRequest(const QString &code,
                                            const QString &message) {
    m_timeout.stop();
    clearRequest();
    setState(State::Idle);
    emit failed(code, message);
}

void SessionRequestCoordinator::clearRequest() {
    m_targetDeviceId.clear();
    m_targetPublicKey.clear();
    m_requestId.clear();
    m_sessionId.clear();
    m_signedRequestPayload.clear();
    m_mode = SessionMode::ViewOnly;
}
