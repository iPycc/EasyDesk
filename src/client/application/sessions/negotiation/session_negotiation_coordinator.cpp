#include "sessions/negotiation/session_negotiation_coordinator.h"

#include "media/webrtc/media_runtime_library.h"
#include "media/webrtc/media_session.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"

#include <QDateTime>

#include <utility>

namespace {

constexpr int kTurnCredentialTimeoutMilliseconds = 15000;

}  // namespace

SessionNegotiationCoordinator::SessionNegotiationCoordinator(
    MediaRuntimeLibrary &media,
    SignalingClient &signaling,
    QObject *parent)
    : QObject(parent), m_media(media), m_signaling(signaling) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionNegotiationCoordinator::handleTimeout);
    connect(&m_signaling, &SignalingClient::messageReceived,
            this, &SessionNegotiationCoordinator::handleMessage);
    connect(&m_signaling, &SignalingClient::stateChanged,
            this, &SessionNegotiationCoordinator::handleSignalingStateChanged);
}

SessionNegotiationCoordinator::~SessionNegotiationCoordinator() = default;

SessionNegotiationCoordinator::State SessionNegotiationCoordinator::state() const {
    return m_state;
}

SessionNegotiationContext SessionNegotiationCoordinator::context() const {
    return m_context;
}

SessionTurnConfiguration SessionNegotiationCoordinator::turnConfiguration() const {
    return m_turn;
}

bool SessionNegotiationCoordinator::begin(
    const SessionNegotiationContext &context) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_state != State::Idle || !m_signaling.isRegistered() ||
        !context.isValid() || context.expiresAt <= now) {
        emit failed(QStringLiteral("E_NEGOTIATION_CONTEXT"),
                    QStringLiteral("Session negotiation context is invalid."));
        return false;
    }
    m_context = context;
    m_turn = {};
    setState(State::AwaitingTurnCredentials);
    m_timeout.start(kTurnCredentialTimeoutMilliseconds);
    return true;
}

void SessionNegotiationCoordinator::stop() {
    m_timeout.stop();
    m_mediaSession.reset();
    m_context = {};
    m_turn = {};
    setState(State::Idle);
}

void SessionNegotiationCoordinator::handleMessage(const QByteArray &message) {
    if (m_state != State::AwaitingTurnCredentials)
        return;

    easydesk::client::signaling::BootstrapMessage bootstrap;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(
            message, bootstrap, error) ||
        bootstrap.type != QStringLiteral("turn.credentials")) {
        return;
    }

    SessionTurnConfiguration configuration;
    if (!bindSessionTurnCredentials(message,
                                    m_context,
                                    QDateTime::currentSecsSinceEpoch(),
                                    configuration,
                                    error)) {
        fail(QStringLiteral("E_TURN_CREDENTIALS"), error);
        return;
    }
    m_timeout.stop();
    m_turn = std::move(configuration);
    if (!startMediaSession(error)) {
        fail(QStringLiteral("E_MEDIA_START"), error);
        return;
    }
    setState(m_context.offerer ? State::AwaitingLocalOffer
                               : State::AwaitingRemoteOffer);
    emit relayReady();
    emit mediaStarted();
}

void SessionNegotiationCoordinator::handleSignalingStateChanged() {
    if (m_state != State::Idle && !m_signaling.isRegistered()) {
        fail(QStringLiteral("E_CONNECTION_STATE"),
             QStringLiteral("Signaling connection was interrupted."));
    }
}

void SessionNegotiationCoordinator::handleTimeout() {
    if (m_state == State::AwaitingTurnCredentials) {
        fail(QStringLiteral("E_TURN_TIMEOUT"),
             QStringLiteral("TURN credentials were not received in time."));
    }
}

bool SessionNegotiationCoordinator::startMediaSession(QString &error) {
    MediaSessionConfig config;
    config.sessionId = m_context.sessionId;
    config.sessionEpoch = m_context.sessionEpoch;
    config.permissionEpoch = m_context.permissionEpoch;
    config.role = m_context.role;
    config.capabilities = m_context.capabilities;
    config.iceGeneration = 1;
    config.iceServers.reserve(m_turn.servers.size());
    for (const SessionTurnServer &server : m_turn.servers) {
        config.iceServers.append(
            {server.url, server.username, server.credential});
    }

    m_mediaSession = m_media.createSession(config);
    if (!m_mediaSession) {
        error = m_media.errorString();
        return false;
    }
    if (m_context.offerer && !m_mediaSession->createOffer()) {
        error = m_mediaSession->errorString();
        m_mediaSession.reset();
        return false;
    }
    error.clear();
    return true;
}

void SessionNegotiationCoordinator::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SessionNegotiationCoordinator::fail(const QString &code,
                                         const QString &message) {
    stop();
    emit failed(code, message);
}
