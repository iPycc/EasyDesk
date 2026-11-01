#include "sessions/negotiation/session_negotiation_coordinator.h"

#include "media/webrtc/media_runtime_library.h"
#include "media/webrtc/media_session.h"
#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"
#include "network/signaling/protocol/session/description/session_description_codec.h"
#include "network/signaling/protocol/session/description/session_description_decoder.h"

#include <QDateTime>

#include <utility>

namespace {

constexpr int kTurnCredentialTimeoutMilliseconds = 15000;
constexpr int kDescriptionTimeoutMilliseconds = 15000;
constexpr qint64 kDescriptionLifetimeSeconds = 45;

}  // namespace

SessionNegotiationCoordinator::SessionNegotiationCoordinator(
    MediaRuntimeLibrary &media,
    WindowsIdentityStore &identity,
    SignalingClient &signaling,
    QObject *parent)
    : QObject(parent),
      m_media(media),
      m_identity(identity),
      m_signaling(signaling) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionNegotiationCoordinator::handleTimeout);
    connect(&m_signaling, &SignalingClient::messageReceived,
            this, &SessionNegotiationCoordinator::handleMessage);
    connect(&m_signaling, &SignalingClient::stateChanged,
            this, &SessionNegotiationCoordinator::handleSignalingStateChanged);
    connect(&m_signaling, &SignalingClient::errorOccurred,
            this, &SessionNegotiationCoordinator::handleSignalingError);
    connect(&m_eventPump, &SessionMediaEventPump::localDescription,
            this, &SessionNegotiationCoordinator::handleLocalDescription);
    connect(&m_eventPump, &SessionMediaEventPump::remoteDescriptionApplied,
            this, &SessionNegotiationCoordinator::handleRemoteDescriptionApplied);
    connect(&m_eventPump, &SessionMediaEventPump::failed,
            this, [this](const QString &message) {
                fail(QStringLiteral("E_MEDIA_EVENT"), message);
            });
    connect(&m_eventPump, &SessionMediaEventPump::sessionClosed,
            this, [this] {
                fail(QStringLiteral("E_MEDIA_CLOSED"),
                     QStringLiteral("The media session closed unexpectedly."));
            });
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
    m_eventPump.stop();
    m_mediaSession.reset();
    m_context = {};
    m_turn = {};
    m_signedLocalDescriptionPayload.clear();
    m_localDescriptionSequence = 0;
    m_signedRemoteDescriptionPayload.clear();
    m_remoteDescriptionSequence = 0;
    m_pendingLocalAnswer.clear();
    setState(State::Idle);
}

void SessionNegotiationCoordinator::handleMessage(const QByteArray &message) {
    easydesk::client::signaling::BootstrapMessage bootstrap;
    QString error;
    if (!easydesk::client::signaling::decodeBootstrapMessage(
            message, bootstrap, error)) {
        return;
    }
    if (m_state == State::AwaitingTurnCredentials &&
        bootstrap.type == QStringLiteral("turn.credentials")) {
        handleTurnCredentials(message);
    } else if (m_state == State::AwaitingRemoteOffer &&
               bootstrap.type == QStringLiteral("session.offer")) {
        handleRemoteOffer(message);
    }
}

void SessionNegotiationCoordinator::handleTurnCredentials(
    const QByteArray &message) {
    QString error;
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
    m_timeout.start(kDescriptionTimeoutMilliseconds);
    emit relayReady();
    emit mediaStarted();
}

void SessionNegotiationCoordinator::handleRemoteOffer(
    const QByteArray &message) {
    if (m_context.offerer || !m_mediaSession) {
        fail(QStringLiteral("E_PROTOCOL"),
             QStringLiteral("Unexpected remote session offer."));
        return;
    }

    easydesk::client::signaling::SessionDescriptionContext description;
    description.requestId = m_context.requestId;
    description.sessionId = m_context.sessionId;
    description.sessionEpoch = m_context.sessionEpoch;
    description.offererDeviceId = m_context.peerDeviceId;
    description.answererDeviceId = m_context.localDeviceId;
    description.signedAcceptPayload = m_context.signedAcceptPayload;

    easydesk::client::signaling::DecodedSessionOffer offer;
    QString error;
    if (!easydesk::client::signaling::decodeSessionOfferEnvelope(
            message, description, offer, error)) {
        fail(QStringLiteral("E_PROTOCOL"), error);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (offer.fields.route.iceGeneration != 1 ||
        offer.fields.route.sequence != 2 ||
        offer.fields.route.expiresAt < now ||
        offer.fields.route.expiresAt > now + kDescriptionLifetimeSeconds ||
        offer.fields.route.expiresAt > m_context.expiresAt) {
        fail(QStringLiteral("E_EXPIRED"),
             QStringLiteral("Remote session offer lifetime is invalid."));
        return;
    }
    if (!m_media.verifyEd25519(m_context.peerPublicKey,
                               offer.signedPayload,
                               offer.signature)) {
        fail(QStringLiteral("E_SIGNATURE"), m_media.errorString());
        return;
    }
    if (!m_mediaSession->setRemoteDescription(
            offer.fields.route.iceGeneration,
            EASYDESK_MEDIA_SDP_OFFER,
            offer.fields.sdp.toUtf8())) {
        fail(QStringLiteral("E_REMOTE_DESCRIPTION"),
             m_mediaSession->errorString());
        return;
    }

    m_signedRemoteDescriptionPayload = offer.signedPayload;
    m_remoteDescriptionSequence = offer.fields.route.sequence;
    setState(State::ApplyingRemoteOffer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
}

void SessionNegotiationCoordinator::handleSignalingStateChanged() {
    if (m_state != State::Idle && !m_signaling.isRegistered()) {
        fail(QStringLiteral("E_CONNECTION_STATE"),
             QStringLiteral("Signaling connection was interrupted."));
    }
}

void SessionNegotiationCoordinator::handleSignalingError(
    const QString &code,
    const QString &message) {
    if (m_state != State::Idle)
        fail(code, message);
}

void SessionNegotiationCoordinator::handleTimeout() {
    if (m_state == State::AwaitingTurnCredentials) {
        fail(QStringLiteral("E_TURN_TIMEOUT"),
             QStringLiteral("TURN credentials were not received in time."));
    } else if (m_state == State::AwaitingLocalOffer ||
               m_state == State::AwaitingRemoteOffer ||
               m_state == State::ApplyingRemoteOffer ||
               m_state == State::AwaitingLocalAnswer ||
               m_state == State::AwaitingRemoteAnswer) {
        fail(QStringLiteral("E_DESCRIPTION_TIMEOUT"),
             QStringLiteral("Session description negotiation timed out."));
    }
}

void SessionNegotiationCoordinator::handleLocalDescription(
    quint32 iceGeneration,
    quint32 sdpType,
    const QByteArray &sdp) {
    const bool localOffer = m_state == State::AwaitingLocalOffer &&
                            m_context.offerer &&
                            sdpType == EASYDESK_MEDIA_SDP_OFFER;
    const bool localAnswer = m_state == State::AwaitingLocalAnswer &&
                             !m_context.offerer &&
                             sdpType == EASYDESK_MEDIA_SDP_ANSWER;
    if ((!localOffer && !localAnswer) || iceGeneration != 1) {
        fail(QStringLiteral("E_MEDIA_EVENT"),
             QStringLiteral("Unexpected local session description."));
        return;
    }

    const QString description = QString::fromUtf8(sdp);
    if (description.toUtf8() != sdp) {
        fail(QStringLiteral("E_MEDIA_EVENT"),
             QStringLiteral("Local session description is not valid UTF-8."));
        return;
    }
    if (localAnswer) {
        m_timeout.stop();
        m_pendingLocalAnswer = sdp;
        setState(State::LocalAnswerReady);
        emit localAnswerReady();
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    easydesk::client::signaling::SessionOfferFields fields;
    fields.route.requestId = m_context.requestId;
    fields.route.sessionId = m_context.sessionId;
    fields.route.sessionEpoch = m_context.sessionEpoch;
    fields.route.iceGeneration = iceGeneration;
    fields.route.sequence = 2;
    fields.route.expiresAt =
        qMin(now + kDescriptionLifetimeSeconds, m_context.expiresAt);
    fields.route.fromDeviceId = m_context.localDeviceId;
    fields.route.toDeviceId = m_context.peerDeviceId;
    fields.signedAcceptPayload = m_context.signedAcceptPayload;
    fields.nonce = m_context.acceptanceNonce;
    fields.sdp = description;

    QString error;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionOfferSigningBytes(fields, error);
    if (signingBytes.isEmpty()) {
        fail(QStringLiteral("E_PROTOCOL"), error);
        return;
    }
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signature.isEmpty()) {
        fail(QStringLiteral("E_SIGNATURE"), m_identity.errorString());
        return;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionOfferEnvelope(
            fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        fail(QStringLiteral("E_OFFER_SEND"),
             error.isEmpty()
                 ? QStringLiteral("Session offer could not be sent.")
                 : error);
        return;
    }

    m_signedLocalDescriptionPayload = signingBytes;
    m_localDescriptionSequence = fields.route.sequence;
    setState(State::AwaitingRemoteAnswer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
    emit localOfferSent();
}

void SessionNegotiationCoordinator::handleRemoteDescriptionApplied(
    quint32 iceGeneration,
    quint32 sdpType) {
    if (m_state != State::ApplyingRemoteOffer || m_context.offerer ||
        !m_mediaSession || iceGeneration != 1 ||
        sdpType != EASYDESK_MEDIA_SDP_OFFER) {
        fail(QStringLiteral("E_MEDIA_EVENT"),
             QStringLiteral("Unexpected remote description confirmation."));
        return;
    }
    if (!m_mediaSession->createAnswer()) {
        fail(QStringLiteral("E_ANSWER_CREATE"),
             m_mediaSession->errorString());
        return;
    }
    setState(State::AwaitingLocalAnswer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
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
    if (!m_eventPump.start(*m_mediaSession)) {
        error = QStringLiteral("Media event pump could not be started.");
        m_mediaSession.reset();
        return false;
    }
    if (m_context.offerer && !m_mediaSession->createOffer()) {
        error = m_mediaSession->errorString();
        m_eventPump.stop();
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
