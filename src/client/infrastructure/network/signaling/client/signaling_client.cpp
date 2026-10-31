#include "network/signaling/client/signaling_client.h"

#include "data/settings/identity/device_profile_store.h"
#include "identity/windows_identity_store.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"
#include "network/signaling/protocol/registration/registration_codec.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>

namespace {

constexpr int kMaximumCollisionRetries = 3;

bool isFatalRegistrationError(const QString &code) {
    return code == QStringLiteral("E_PROTOCOL_VERSION") ||
           code == QStringLiteral("E_SIGNATURE") ||
           code == QStringLiteral("E_TIMESTAMP") ||
           code == QStringLiteral("E_IDENTITY_CONFLICT");
}

}  // namespace

SignalingClient::SignalingClient(DeviceProfileStore &profile,
                                 WindowsIdentityStore &identity,
                                 QObject *parent)
    : QObject(parent), m_profile(profile), m_identity(identity) {
    m_reconnectTimer.setSingleShot(true);
    m_assistanceExpiryTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SignalingClient::connectNow);
    connect(&m_assistanceExpiryTimer, &QTimer::timeout,
            this, &SignalingClient::clearAssistanceCode);
    connect(&m_transport, &WinHttpWebSocketTransport::stateChanged,
            this, &SignalingClient::handleTransportState);
    connect(&m_transport, &WinHttpWebSocketTransport::textReceived,
            this, &SignalingClient::handleMessage);
    connect(&m_transport, &WinHttpWebSocketTransport::errorOccurred,
            this, [this](const QString &message) {
                emit errorOccurred(QStringLiteral("E_TRANSPORT"), message);
            });
}

SignalingClient::~SignalingClient() {
    m_shouldRun = false;
    m_reconnectTimer.stop();
    m_assistanceExpiryTimer.stop();
}

SignalingClient::State SignalingClient::state() const {
    return m_state;
}

bool SignalingClient::isRegistered() const {
    return m_state == State::Registered;
}

QString SignalingClient::assistanceCode() const {
    return m_assistanceCode;
}

qint64 SignalingClient::assistanceCodeExpiresAt() const {
    return m_assistanceExpiresAt;
}

bool SignalingClient::start(const QUrl &endpoint, bool allowInsecureLoopback) {
    const QString scheme = endpoint.scheme().toLower();
    if (m_shouldRun || (scheme != QStringLiteral("ws") && scheme != QStringLiteral("wss")) ||
        endpoint.host().isEmpty() || !m_identity.isReady() ||
        m_profile.profile().deviceId.isEmpty()) {
        return false;
    }
    m_endpoint = endpoint;
    m_allowInsecureLoopback = allowInsecureLoopback;
    m_shouldRun = true;
    m_retryAttempt = 0;
    m_collisionRetries = 0;
    m_retryImmediately = false;
    connectNow();
    return true;
}

void SignalingClient::stop() {
    m_shouldRun = false;
    m_retryImmediately = false;
    m_reconnectTimer.stop();
    m_assistanceExpiryTimer.stop();
    clearAssistanceCode();
    if (m_transport.state() != WinHttpWebSocketTransport::State::Disconnected)
        m_transport.disconnectFromServer();
    setState(State::Offline);
}

bool SignalingClient::refreshAssistanceCode() {
    if (!isRegistered())
        return false;
    return m_transport.sendText(
        easydesk::client::signaling::encodeAssistanceRefresh());
}

bool SignalingClient::sendRegisteredMessage(const QByteArray &message) {
    return isRegistered() && !message.isEmpty() && m_transport.sendText(message);
}

void SignalingClient::connectNow() {
    if (!m_shouldRun)
        return;
    if (m_transport.state() != WinHttpWebSocketTransport::State::Disconnected)
        return;
    setState(State::Connecting);
    if (!m_transport.connectToServer(m_endpoint, m_allowInsecureLoopback))
        scheduleReconnect();
}

void SignalingClient::handleTransportState(WinHttpWebSocketTransport::State state) {
    switch (state) {
    case WinHttpWebSocketTransport::State::Connecting:
        setState(State::Connecting);
        break;
    case WinHttpWebSocketTransport::State::Connected:
        setState(State::Registering);
        break;
    case WinHttpWebSocketTransport::State::Closing:
        break;
    case WinHttpWebSocketTransport::State::Disconnected:
        clearAssistanceCode();
        if (m_shouldRun)
            scheduleReconnect();
        else
            setState(State::Offline);
        break;
    }
}

void SignalingClient::handleMessage(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        failProtocol(error);
        return;
    }

    using Kind = easydesk::client::signaling::BootstrapMessage::Kind;
    switch (decoded.kind) {
    case Kind::RegistrationChallenge:
        handleChallenge(message);
        break;
    case Kind::RegistrationResult:
        handleRegistrationResult(message);
        break;
    case Kind::AssistanceCode:
        handleAssistanceCode(message);
        break;
    case Kind::AssistanceResolved:
        if (isRegistered())
            emit messageReceived(message);
        else
            failProtocol(QStringLiteral("Assistance identity arrived before registration completed."));
        break;
    case Kind::Error:
        handleServerError(message);
        break;
    case Kind::Other:
        if (isRegistered())
            emit messageReceived(message);
        else
            failProtocol(QStringLiteral("Unexpected signaling message before registration."));
        break;
    }
}

void SignalingClient::handleChallenge(const QByteArray &message) {
    if (m_state != State::Registering) {
        failProtocol(QStringLiteral("Registration challenge arrived in the wrong state."));
        return;
    }

    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        failProtocol(error);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (decoded.challenge.expiresAt < now) {
        failProtocol(QStringLiteral("Registration challenge expired before it could be signed."));
        return;
    }

    const DeviceProfile &profile = m_profile.profile();
    easydesk::client::signaling::RegistrationFields fields;
    fields.deviceId = profile.deviceId;
    fields.publicKey = m_identity.publicKey();
    fields.displayName = profile.displayName;
    fields.platform = QStringLiteral("Windows");
    fields.schemaVersion = 1;
    fields.timestamp = now;
    fields.nonce = decoded.challenge.nonce;
    const QByteArray signingBytes =
        easydesk::client::signaling::registrationSigningBytes(fields, error);
    if (signingBytes.isEmpty()) {
        failProtocol(error);
        return;
    }
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signature.isEmpty()) {
        failProtocol(m_identity.errorString());
        return;
    }
    const QByteArray response =
        easydesk::client::signaling::encodeRegistrationResponse(fields, signature, error);
    if (response.isEmpty() || !m_transport.sendText(response)) {
        failProtocol(error.isEmpty()
                         ? QStringLiteral("Registration response could not be sent.")
                         : error);
    }
}

void SignalingClient::handleRegistrationResult(const QByteArray &message) {
    if (m_state != State::Registering) {
        failProtocol(QStringLiteral("Registration result arrived in the wrong state."));
        return;
    }
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        failProtocol(error);
        return;
    }
    const DeviceProfile &profile = m_profile.profile();
    const QString expectedFingerprint = QString::fromLatin1(
        QCryptographicHash::hash(m_identity.publicKey(), QCryptographicHash::Sha256).toHex());
    if (decoded.registration.deviceId != profile.deviceId ||
        decoded.registration.fingerprint != expectedFingerprint) {
        failProtocol(QStringLiteral("Registration result does not match the local identity."));
        return;
    }
    m_retryAttempt = 0;
    m_collisionRetries = 0;
    setState(State::Registered);
}

void SignalingClient::handleAssistanceCode(const QByteArray &message) {
    if (!isRegistered()) {
        failProtocol(QStringLiteral("Assistance code arrived before registration completed."));
        return;
    }
    easydesk::client::signaling::BootstrapMessage decoded;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, error)) {
        failProtocol(error);
        return;
    }
    m_assistanceCode = decoded.assistance.code;
    m_assistanceExpiresAt = QDateTime::currentSecsSinceEpoch() +
                            decoded.assistance.expiresInSeconds;
    m_assistanceExpiryTimer.start(decoded.assistance.expiresInSeconds * 1000);
    emit assistanceCodeChanged();
}

void SignalingClient::handleServerError(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage decoded;
    QString parseError;
    if (!easydesk::client::signaling::decodeBootstrapMessage(message, decoded, parseError)) {
        failProtocol(parseError);
        return;
    }

    emit errorOccurred(decoded.error.code, decoded.error.message);
    if (decoded.error.code == QStringLiteral("E_ID_COLLISION") &&
        m_collisionRetries < kMaximumCollisionRetries) {
        if (!m_profile.rotateDeviceId()) {
            m_shouldRun = false;
            emit errorOccurred(QStringLiteral("E_PROFILE"), m_profile.errorString());
        } else {
            ++m_collisionRetries;
            m_retryImmediately = true;
            emit profileChanged();
        }
    } else if (isFatalRegistrationError(decoded.error.code) ||
               decoded.error.code == QStringLiteral("E_ID_COLLISION")) {
        m_shouldRun = false;
        m_transport.disconnectFromServer();
    }
}

void SignalingClient::scheduleReconnect() {
    if (!m_shouldRun || m_reconnectTimer.isActive())
        return;
    if (m_retryImmediately) {
        m_retryImmediately = false;
        setState(State::RetryWaiting);
        m_reconnectTimer.start(0);
        return;
    }

    const int exponent = qMin(m_retryAttempt, 5);
    const int baseSeconds = qMin(1 << exponent, 30);
    m_retryAttempt = qMin(m_retryAttempt + 1, 5);
    const int baseMilliseconds = baseSeconds * 1000;
    const int jitter = baseMilliseconds / 5;
    const int delay = baseMilliseconds -
                      QRandomGenerator::system()->bounded(jitter + 1);
    setState(State::RetryWaiting);
    m_reconnectTimer.start(delay);
}

void SignalingClient::clearAssistanceCode() {
    if (m_assistanceCode.isEmpty() && m_assistanceExpiresAt == 0)
        return;
    m_assistanceCode.clear();
    m_assistanceExpiresAt = 0;
    m_assistanceExpiryTimer.stop();
    emit assistanceCodeChanged();
}

void SignalingClient::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SignalingClient::failProtocol(const QString &message) {
    m_shouldRun = false;
    emit errorOccurred(QStringLiteral("E_PROTOCOL"), message);
    m_transport.disconnectFromServer();
}
