#include "sessions/negotiation/session_negotiation_coordinator.h"

#include "media/webrtc/media_runtime_library.h"
#include "media/webrtc/media_session.h"
#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"
#include "network/signaling/protocol/session/description/session_description_codec.h"
#include "network/signaling/protocol/session/description/session_description_decoder.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace {

constexpr int kTurnCredentialTimeoutMilliseconds = 15000;
constexpr int kDescriptionTimeoutMilliseconds = 15000;
constexpr int kIceConnectionTimeoutMilliseconds = 30000;
constexpr int kRecoveryTimeoutMilliseconds = 15000;
constexpr int kReadyTimeoutMilliseconds = 15000;
constexpr int kRemoteEndGraceMilliseconds = 1500;
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
      m_signaling(signaling),
      m_iceSender(identity, signaling, this),
      m_iceReceiver(media, this),
      m_terminationReceiver(media, this),
      m_terminationSender(identity, signaling, this) {
    m_timeout.setSingleShot(true);
    m_readyTimeout.setSingleShot(true);
    connect(&m_readyTimeout, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("E_SESSION_READY_TIMEOUT"),
             QStringLiteral("安全会话验证超时"));
    });
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionNegotiationCoordinator::handleTimeout);
    connect(&m_authorizationDeadline, &SessionAuthorizationDeadline::expired,
            this, [this] {
                fail(QStringLiteral("E_SESSION_AUTHORIZATION_EXPIRED"),
                     QStringLiteral("远程会话授权已到期"));
            });
    connect(&m_signaling, &SignalingClient::messageReceived,
            this, &SessionNegotiationCoordinator::handleMessage);
    connect(&m_signaling, &SignalingClient::stateChanged,
            this, &SessionNegotiationCoordinator::handleSignalingStateChanged);
    connect(&m_signaling, &SignalingClient::errorOccurred,
            this, &SessionNegotiationCoordinator::handleSignalingError);
    connect(&m_eventPump, &SessionMediaEventPump::localDescription,
            this, &SessionNegotiationCoordinator::handleLocalDescription);
    connect(&m_eventPump, &SessionMediaEventPump::localCandidate,
            &m_iceSender, &SessionIceSignalSender::enqueueCandidate);
    connect(&m_eventPump, &SessionMediaEventPump::gatheringComplete,
            &m_iceSender, &SessionIceSignalSender::markGatheringComplete);
    connect(&m_eventPump, &SessionMediaEventPump::connectionStateChanged,
            &m_transportState,
            &SessionTransportState::updatePeerConnectionState);
    connect(&m_eventPump, &SessionMediaEventPump::iceConnectionStateChanged,
            &m_transportState,
            &SessionTransportState::updateIceConnectionState);
    connect(&m_eventPump, &SessionMediaEventPump::remoteDescriptionApplied,
            this, &SessionNegotiationCoordinator::handleRemoteDescriptionApplied);
    connect(&m_eventPump, &SessionMediaEventPump::captureGeometryChanged,
            this, [this](const QRect &region, quint64 generation, bool valid) {
                if (m_context.role != SessionRole::Controlled) {
                    fail(QStringLiteral("E_CAPTURE_GEOMETRY"),
                         QStringLiteral("Unexpected capture geometry event."));
                    return;
                }
                const bool supported = valid && region.width() <= 65535 && region.height() <= 65535;
                m_localCaptureGeometry = {generation, supported ? region.size() : QSize(0, 0), supported};
                emit captureGeometryChanged(supported ? region : QRect(), generation, supported);
                publishCaptureGeometry();
            });
    connect(&m_eventPump, &SessionMediaEventPump::dataChannelOpened,
            this, [this](const QByteArray &label) {
                m_inputTransport.setChannelOpen(label, true);
                if (label == "control.v1")
                    handleControlOpen();
            });
    connect(&m_eventPump, &SessionMediaEventPump::dataChannelMessage,
            this, [this](const QByteArray &label, const QByteArray &message) {
                if (label == "control.v1")
                    handleControlMessage(message);
                else
                    m_inputTransport.receive(label, message);
            });
    connect(&m_eventPump, &SessionMediaEventPump::dataChannelClosed,
            this, [this](const QByteArray &label) {
                m_inputTransport.setChannelOpen(label, false);
                if (label != "control.v1" || m_state == State::Idle || m_state == State::Ending)
                    return;
                if (!m_handshakeComplete) {
                    fail(QStringLiteral("E_CONTROL_CHANNEL_CLOSED"),
                         QStringLiteral("安全会话通道已关闭"));
                    return;
                }
                setState(State::Ending);
                closeMediaSession(true);
                m_timeout.start(kRemoteEndGraceMilliseconds);
            });
    connect(&m_inputTransport, &SessionInputTransport::availabilityChanged,
            this, &SessionNegotiationCoordinator::inputAvailabilityChanged);
    connect(&m_inputTransport, &SessionInputTransport::messageReceived,
            this, &SessionNegotiationCoordinator::inputMessageReceived);
    connect(&m_eventPump, &SessionMediaEventPump::failed,
            this, [this](const QString &message) {
                fail(QStringLiteral("E_MEDIA_EVENT"), message);
            });
    connect(&m_eventPump, &SessionMediaEventPump::sessionClosed,
            this, [this] {
                fail(QStringLiteral("E_MEDIA_CLOSED"),
                     QStringLiteral("The media session closed unexpectedly."));
            });
    connect(&m_iceSender, &SessionIceSignalSender::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
            });
    connect(&m_iceSender, &SessionIceSignalSender::candidatesSent,
            this, [this](int, quint64 sequence) {
                QString error;
                if (!m_terminationSender.noteLocalSequence(sequence, error))
                    fail(QStringLiteral("E_LOCAL_SEQUENCE"), error);
            });
    connect(&m_iceSender, &SessionIceSignalSender::gatheringCompleteSent,
            this, [this](quint64 sequence) {
                QString error;
                if (!m_terminationSender.noteLocalSequence(sequence, error))
                    fail(QStringLiteral("E_LOCAL_SEQUENCE"), error);
            });
    connect(&m_iceReceiver, &SessionIceSignalReceiver::candidateBatchApplied,
            this, [this](int count, quint64 sequence) {
                QString error;
                if (!m_terminationReceiver.noteRemoteSequence(sequence, error)) {
                    fail(QStringLiteral("E_REMOTE_SEQUENCE"), error);
                    return;
                }
                emit remoteCandidatesApplied(count, sequence);
            });
    connect(&m_iceReceiver,
            &SessionIceSignalReceiver::gatheringCompleteReceived,
            this, [this](quint64 sequence) {
                QString error;
                if (!m_terminationReceiver.noteRemoteSequence(sequence, error)) {
                    fail(QStringLiteral("E_REMOTE_SEQUENCE"), error);
                    return;
                }
                emit remoteGatheringComplete(sequence);
            });
    connect(&m_iceReceiver, &SessionIceSignalReceiver::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
            });
    connect(&m_transportState, &SessionTransportState::stateChanged,
            this, &SessionNegotiationCoordinator::transportStateChanged);
    connect(&m_transportState, &SessionTransportState::stateChanged,
            this, [this](SessionTransportState::State state) {
                if (state != SessionTransportState::State::Connected)
                    m_videoMonitor.transportDisconnected();
                if (state != SessionTransportState::State::Connected)
                    m_inputTransport.setSessionReady(false);
                if (state == SessionTransportState::State::Disconnected &&
                    m_transportEverConnected && m_state == State::AwaitingIce &&
                    !m_timeout.isActive())
                    m_timeout.start(kRecoveryTimeoutMilliseconds);
            });
    connect(&m_transportState, &SessionTransportState::connected,
            this, [this] {
                m_transportEverConnected = true;
                if (m_state == State::AwaitingIce)
                    m_timeout.stop();
                if (!m_handshakeComplete && !m_readyTimeout.isActive())
                    m_readyTimeout.start(kReadyTimeoutMilliseconds);
                m_videoMonitor.transportConnected();
                m_inputTransport.setSessionReady(m_handshakeComplete);
                publishCaptureGeometry();
                if (m_state != State::Idle)
                    emit transportConnected();
            });
    connect(&m_videoMonitor, &SessionVideoMonitor::firstFrame,
            this, &SessionNegotiationCoordinator::firstFramePresented);
    connect(&m_videoMonitor, &SessionVideoMonitor::framePresented,
            this, &SessionNegotiationCoordinator::framePresented);
    connect(&m_videoMonitor, &SessionVideoMonitor::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
            });
    connect(&m_transportState, &SessionTransportState::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
            });
    connect(&m_terminationReceiver, &SessionTerminationReceiver::ended,
            this, [this](
                      easydesk::client::signaling::SessionEndReason reason,
                      quint64 sequence) {
                stop();
                emit remoteSessionEnded(reason, sequence);
            });
    connect(&m_terminationReceiver, &SessionTerminationReceiver::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
            });
    connect(&m_terminationSender, &SessionTerminationSender::sent,
            this, [this](
                      easydesk::client::signaling::SessionEndReason reason,
                      quint64 sequence) {
                emit localSessionEndSent(reason, sequence);
            });
    connect(&m_terminationSender, &SessionTerminationSender::delivered,
            this, [this](
                      easydesk::client::signaling::SessionEndReason reason,
                      quint64 sequence) {
                stop();
                emit localSessionEnded(reason, sequence);
            });
    connect(&m_terminationSender, &SessionTerminationSender::failed,
            this, [this](const QString &code, const QString &message) {
                fail(code, message);
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

SessionTransportState::State
SessionNegotiationCoordinator::transportState() const {
    return m_transportState.state();
}

bool SessionNegotiationCoordinator::hasPresentedFrame() const {
    return m_videoMonitor.firstFramePresented();
}

QSize SessionNegotiationCoordinator::presentedFrameSize() const {
    return m_videoMonitor.presentedFrameSize();
}

QSize SessionNegotiationCoordinator::presentedTargetSize() const {
    return m_videoMonitor.presentedTargetSize();
}

RemoteCaptureGeometry SessionNegotiationCoordinator::remoteCaptureGeometry() const {
    return m_remoteCaptureGeometry;
}

bool SessionNegotiationCoordinator::isSessionReady() const {
    return m_handshakeComplete && m_controlOpen && m_readyExchange.isReady() &&
           m_transportState.state() == SessionTransportState::State::Connected;
}

bool SessionNegotiationCoordinator::isInputChannelOpen(InputStream stream) const {
    return m_inputTransport.isOpen(stream);
}

SessionInputTransport::SendResult SessionNegotiationCoordinator::sendInputMessage(
    InputStream stream, const QByteArray &message) {
    return m_inputTransport.send(stream, message);
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
    QString error;
    if (!m_iceSender.begin(context, 1, error)) {
        m_context = {};
        emit failed(QStringLiteral("E_ICE_SENDER"), error);
        return false;
    }
    if (!m_terminationReceiver.begin(context, error)) {
        m_iceSender.stop();
        m_context = {};
        emit failed(QStringLiteral("E_TERMINATION_RECEIVER"), error);
        return false;
    }
    if (!m_terminationSender.begin(context, error)) {
        m_terminationReceiver.stop();
        m_iceSender.stop();
        m_context = {};
        emit failed(QStringLiteral("E_TERMINATION_SENDER"), error);
        return false;
    }
    if (!m_authorizationDeadline.start(context.expiresAt)) {
        m_terminationSender.stop();
        m_terminationReceiver.stop();
        m_iceSender.stop();
        m_context = {};
        emit failed(QStringLiteral("E_SESSION_AUTHORIZATION_EXPIRED"),
                    QStringLiteral("远程会话授权已到期"));
        return false;
    }
    if (!m_readyExchange.start(context, error)) {
        m_authorizationDeadline.stop();
        m_terminationSender.stop();
        m_terminationReceiver.stop();
        m_iceSender.stop();
        m_context = {};
        emit failed(QStringLiteral("E_SESSION_READY"), error);
        return false;
    }
    setState(State::AwaitingTurnCredentials);
    m_timeout.start(kTurnCredentialTimeoutMilliseconds);
    return true;
}

bool SessionNegotiationCoordinator::endSession(
    easydesk::client::signaling::SessionEndReason reason) {
    if (m_state == State::Idle || m_state == State::Ending)
        return false;
    setState(State::Ending);
    closeMediaSession();
    return m_terminationSender.end(reason);
}

bool SessionNegotiationCoordinator::attachVideoRenderer(quintptr nativeWindow,
                                                         QString &error) {
    if (nativeWindow == 0) {
        error = QStringLiteral("Remote video window is invalid.");
        return false;
    }
    m_videoRendererWindow = nativeWindow;
    if (!m_mediaSession || !m_handshakeComplete) {
        error.clear();
        return true;
    }
    return activateVideoRenderer(error);
}

bool SessionNegotiationCoordinator::activateVideoRenderer(QString &error) {
    if (!m_mediaSession || m_videoRendererWindow == 0) {
        error = QStringLiteral("Remote video window is invalid.");
        return false;
    }
    if (!m_mediaSession->attachVideoRenderer(m_videoRendererWindow)) {
        error = m_mediaSession->errorString();
        m_videoRendererWindow = 0;
        return false;
    }
    m_videoMonitor.stop();
    if (!m_videoMonitor.start(*m_mediaSession)) {
        (void)m_mediaSession->detachVideoRenderer();
        m_videoRendererWindow = 0;
        error = QStringLiteral("Video monitor could not be started.");
        return false;
    }
    if (m_transportState.state() == SessionTransportState::State::Connected)
        m_videoMonitor.transportConnected();
    if (!m_mediaSession || m_videoRendererWindow == 0) {
        error = QStringLiteral("The media session ended while attaching the video renderer.");
        return false;
    }
    error.clear();
    return true;
}

bool SessionNegotiationCoordinator::detachVideoRenderer(QString &error) {
    m_videoRendererWindow = 0;
    m_videoMonitor.stop();
    if (!m_mediaSession) {
        error.clear();
        return true;
    }
    if (!m_mediaSession->detachVideoRenderer()) {
        error = m_mediaSession->errorString();
        return false;
    }
    error.clear();
    return true;
}

void SessionNegotiationCoordinator::stop() {
    closeMediaSession();
    m_terminationSender.stop();
    m_context = {};
    m_turn = {};
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
    } else if (m_state == State::AwaitingTurnCredentials &&
               bootstrap.type == QStringLiteral("turn.unavailable")) {
        handleTurnUnavailable(message);
    } else if (bootstrap.type == QStringLiteral("session.candidates")) {
        m_iceReceiver.handleCandidates(message);
    } else if (bootstrap.type ==
               QStringLiteral("session.gathering_complete")) {
        m_iceReceiver.handleGatheringComplete(message);
    } else if (bootstrap.type == QStringLiteral("session.end") &&
               m_terminationReceiver.isActive()) {
        m_terminationReceiver.handleEnd(message);
    } else if (bootstrap.type == QStringLiteral("session.delivered") &&
               m_terminationSender.isAwaitingDelivery()) {
        m_terminationSender.handleDelivery(message);
    } else if (m_state == State::AwaitingRemoteOffer &&
               bootstrap.type == QStringLiteral("session.offer")) {
        handleRemoteOffer(message);
    } else if (m_state == State::AwaitingRemoteAnswer &&
               bootstrap.type == QStringLiteral("session.answer")) {
        handleRemoteAnswer(message);
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
    m_turn = std::move(configuration);
    startConfiguredMedia(true);
}

void SessionNegotiationCoordinator::handleTurnUnavailable(const QByteArray &message) {
    QString error;
    if (!bindSessionTurnUnavailable(message, m_context, error)) {
        fail(QStringLiteral("E_TURN_CONFIGURATION"), error);
        return;
    }
    m_turn = {};
    startConfiguredMedia(false);
}

void SessionNegotiationCoordinator::startConfiguredMedia(bool hasRelay) {
    m_timeout.stop();
    QString error;
    if (!startMediaSession(error)) {
        fail(QStringLiteral("E_MEDIA_START"), error);
        return;
    }
    setState(m_context.offerer ? State::AwaitingLocalOffer
                               : State::AwaitingRemoteOffer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
    if (hasRelay)
        emit relayReady();
    emit mediaStarted();
}

void SessionNegotiationCoordinator::handleControlOpen() {
    if (m_controlOpen || !m_mediaSession) {
        fail(QStringLiteral("E_SESSION_READY"), QStringLiteral("安全会话通道状态无效"));
        return;
    }
    m_controlOpen = true;
    if (!m_readyTimeout.isActive())
        m_readyTimeout.start(kReadyTimeoutMilliseconds);
    QString error;
    const QByteArray ready = m_readyExchange.makeReady(error);
    if (ready.isEmpty()) {
        fail(QStringLiteral("E_SESSION_READY"), error);
        return;
    }
    (void)sendControlMessage(ready);
}

void SessionNegotiationCoordinator::handleControlMessage(const QByteArray &message) {
    if (!m_controlOpen || message.size() > 1024) {
        fail(QStringLiteral("E_SESSION_READY"), QStringLiteral("安全会话消息状态无效"));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("E_SESSION_READY"), QStringLiteral("安全会话消息无效"));
        return;
    }
    const QString type = document.object().value(QStringLiteral("type")).toString();
    if (m_handshakeComplete) {
        if (type == QStringLiteral("capture.geometry")) {
            handleCaptureGeometry(message);
            return;
        }
        fail(QStringLiteral("E_SESSION_READY"), QStringLiteral("未知安全会话消息"));
        return;
    }
    QString error;
    if (type == QStringLiteral("session.ready")) {
        QByteArray acknowledgement;
        if (!m_readyExchange.receiveReady(message, acknowledgement, error)) {
            fail(QStringLiteral("E_SESSION_READY"), error);
            return;
        }
        if (!sendControlMessage(acknowledgement))
            return;
    } else if (type == QStringLiteral("session.ready_ack")) {
        if (!m_readyExchange.receiveAcknowledgement(message, error)) {
            fail(QStringLiteral("E_SESSION_READY"), error);
            return;
        }
    } else {
        fail(QStringLiteral("E_SESSION_READY"), QStringLiteral("未知安全会话消息"));
        return;
    }
    finishReadyExchange();
}

void SessionNegotiationCoordinator::handleCaptureGeometry(const QByteArray &message) {
    RemoteCaptureGeometry geometry;
    QString error;
    if (!decodeCaptureGeometry(message, m_context, geometry, error) ||
        geometry.generation <= m_remoteCaptureGeneration) {
        fail(QStringLiteral("E_CAPTURE_GEOMETRY"),
             error.isEmpty() ? QStringLiteral("Capture geometry generation is stale.") : error);
        return;
    }
    m_remoteCaptureGeneration = geometry.generation;
    m_remoteCaptureGeometry = geometry;
    emit remoteCaptureGeometryChanged(geometry.size, geometry.generation, geometry.valid);
}

void SessionNegotiationCoordinator::publishCaptureGeometry() {
    if (m_context.role != SessionRole::Controlled || !isSessionReady() ||
        m_localCaptureGeometry.generation == 0 ||
        m_localCaptureGeometry.generation <= m_sentCaptureGeneration)
        return;
    QString error;
    const QByteArray message = encodeCaptureGeometry(m_context, m_localCaptureGeometry, error);
    if (message.isEmpty()) {
        fail(QStringLiteral("E_CAPTURE_GEOMETRY"), error);
        return;
    }
    const quint64 generation = m_localCaptureGeometry.generation;
    if (sendControlMessage(message))
        m_sentCaptureGeneration = generation;
}

bool SessionNegotiationCoordinator::sendControlMessage(const QByteArray &message) {
    if (!m_mediaSession || !m_controlOpen || message.isEmpty() ||
        m_mediaSession->sendData("control.v1", message) != MediaSession::SendResult::Sent) {
        fail(QStringLiteral("E_SESSION_READY_SEND"), QStringLiteral("无法发送安全会话消息"));
        return false;
    }
    return true;
}

void SessionNegotiationCoordinator::finishReadyExchange() {
    if (m_handshakeComplete || !m_readyExchange.isReady())
        return;
    m_handshakeComplete = true;
    m_readyTimeout.stop();
    m_inputTransport.setSessionReady(
        m_transportState.state() == SessionTransportState::State::Connected);
    QString error;
    if (m_videoRendererWindow != 0 && !activateVideoRenderer(error)) {
        fail(QStringLiteral("E_VIDEO_RENDERER"), error);
        return;
    }
    emit sessionReady();
    publishCaptureGeometry();
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
    if (!m_iceReceiver.bindRemoteDescription(
            offer.signedPayload,
            offer.fields.route.sequence,
            offer.fields.sdp.toUtf8(),
            error)) {
        fail(QStringLiteral("E_REMOTE_ICE_BINDING"), error);
        return;
    }

    m_signedRemoteDescriptionPayload = offer.signedPayload;
    m_remoteDescriptionSequence = offer.fields.route.sequence;
    if (!m_terminationReceiver.noteRemoteSequence(
            m_remoteDescriptionSequence, error)) {
        fail(QStringLiteral("E_REMOTE_SEQUENCE"), error);
        return;
    }
    setState(State::ApplyingRemoteOffer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
}

void SessionNegotiationCoordinator::handleRemoteAnswer(
    const QByteArray &message) {
    if (!m_context.offerer || !m_mediaSession ||
        m_signedLocalDescriptionPayload.isEmpty()) {
        fail(QStringLiteral("E_PROTOCOL"),
             QStringLiteral("Unexpected remote session answer."));
        return;
    }

    easydesk::client::signaling::SessionDescriptionContext description;
    description.requestId = m_context.requestId;
    description.sessionId = m_context.sessionId;
    description.sessionEpoch = m_context.sessionEpoch;
    description.offererDeviceId = m_context.localDeviceId;
    description.answererDeviceId = m_context.peerDeviceId;
    description.signedAcceptPayload = m_context.signedAcceptPayload;

    easydesk::client::signaling::DecodedSessionAnswer answer;
    QString error;
    if (!easydesk::client::signaling::decodeSessionAnswerEnvelope(
            message,
            description,
            m_signedLocalDescriptionPayload,
            answer,
            error)) {
        fail(QStringLiteral("E_PROTOCOL"), error);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (answer.fields.route.iceGeneration != 1 ||
        answer.fields.route.sequence != 2 ||
        answer.fields.route.expiresAt < now ||
        answer.fields.route.expiresAt > now + kDescriptionLifetimeSeconds ||
        answer.fields.route.expiresAt > m_context.expiresAt) {
        fail(QStringLiteral("E_EXPIRED"),
             QStringLiteral("Remote session answer lifetime is invalid."));
        return;
    }
    if (!m_media.verifyEd25519(m_context.peerPublicKey,
                               answer.signedPayload,
                               answer.signature)) {
        fail(QStringLiteral("E_SIGNATURE"), m_media.errorString());
        return;
    }
    if (!m_mediaSession->setRemoteDescription(
            answer.fields.route.iceGeneration,
            EASYDESK_MEDIA_SDP_ANSWER,
            answer.fields.sdp.toUtf8())) {
        fail(QStringLiteral("E_REMOTE_DESCRIPTION"),
             m_mediaSession->errorString());
        return;
    }
    if (!m_iceReceiver.bindRemoteDescription(
            answer.signedPayload,
            answer.fields.route.sequence,
            answer.fields.sdp.toUtf8(),
            error)) {
        fail(QStringLiteral("E_REMOTE_ICE_BINDING"), error);
        return;
    }

    m_signedRemoteDescriptionPayload = answer.signedPayload;
    m_remoteDescriptionSequence = answer.fields.route.sequence;
    if (!m_terminationReceiver.noteRemoteSequence(
            m_remoteDescriptionSequence, error)) {
        fail(QStringLiteral("E_REMOTE_SEQUENCE"), error);
        return;
    }
    setState(State::ApplyingRemoteAnswer);
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
               m_state == State::AwaitingRemoteAnswer ||
               m_state == State::ApplyingRemoteAnswer) {
        fail(QStringLiteral("E_DESCRIPTION_TIMEOUT"),
             QStringLiteral("Session description negotiation timed out."));
    } else if (m_state == State::AwaitingIce &&
               m_transportState.state() != SessionTransportState::State::Connected) {
        fail(m_transportEverConnected ? QStringLiteral("E_MEDIA_RECOVERY_TIMEOUT")
                                      : QStringLiteral("E_ICE_TIMEOUT"),
             m_transportEverConnected ? QStringLiteral("The media connection did not recover in time.")
                                      : QStringLiteral("The media connection could not be established in time."));
    } else if (m_state == State::Ending) {
        fail(QStringLiteral("E_CONTROL_CHANNEL_CLOSED"),
             QStringLiteral("安全会话通道已关闭"));
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
        sendLocalAnswer(description);
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
    if (!m_terminationSender.noteLocalSequence(
            m_localDescriptionSequence, error)) {
        fail(QStringLiteral("E_LOCAL_SEQUENCE"), error);
        return;
    }
    if (!m_iceSender.bindLocalDescription(
            m_signedLocalDescriptionPayload,
            m_localDescriptionSequence,
            sdp,
            error)) {
        fail(QStringLiteral("E_ICE_BINDING"), error);
        return;
    }
    setState(State::AwaitingRemoteAnswer);
    m_timeout.start(kDescriptionTimeoutMilliseconds);
    emit localOfferSent();
}

void SessionNegotiationCoordinator::handleRemoteDescriptionApplied(
    quint32 iceGeneration,
    quint32 sdpType) {
    const bool remoteOffer = m_state == State::ApplyingRemoteOffer &&
                             !m_context.offerer &&
                             sdpType == EASYDESK_MEDIA_SDP_OFFER;
    const bool remoteAnswer = m_state == State::ApplyingRemoteAnswer &&
                              m_context.offerer &&
                              sdpType == EASYDESK_MEDIA_SDP_ANSWER;
    if ((!remoteOffer && !remoteAnswer) || !m_mediaSession ||
        iceGeneration != 1) {
        fail(QStringLiteral("E_MEDIA_EVENT"),
             QStringLiteral("Unexpected remote description confirmation."));
        return;
    }
    QString error;
    if (!m_iceReceiver.markRemoteDescriptionApplied(iceGeneration, error)) {
        fail(QStringLiteral("E_REMOTE_ICE_APPLY"), error);
        return;
    }
    if (remoteAnswer) {
        m_timeout.stop();
        setState(State::AwaitingIce);
        if (m_transportState.state() != SessionTransportState::State::Connected)
            m_timeout.start(m_transportEverConnected ? kRecoveryTimeoutMilliseconds
                                                     : kIceConnectionTimeoutMilliseconds);
        emit descriptionsExchanged();
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

void SessionNegotiationCoordinator::sendLocalAnswer(const QString &sdp) {
    if (m_state != State::AwaitingLocalAnswer || m_context.offerer ||
        m_signedRemoteDescriptionPayload.isEmpty()) {
        fail(QStringLiteral("E_PROTOCOL"),
             QStringLiteral("Local session answer context is invalid."));
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    easydesk::client::signaling::SessionAnswerFields fields;
    fields.route.requestId = m_context.requestId;
    fields.route.sessionId = m_context.sessionId;
    fields.route.sessionEpoch = m_context.sessionEpoch;
    fields.route.iceGeneration = 1;
    fields.route.sequence = 2;
    fields.route.expiresAt =
        qMin(now + kDescriptionLifetimeSeconds, m_context.expiresAt);
    fields.route.fromDeviceId = m_context.localDeviceId;
    fields.route.toDeviceId = m_context.peerDeviceId;
    fields.signedOfferPayload = m_signedRemoteDescriptionPayload;
    fields.sdp = sdp;

    QString error;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionAnswerSigningBytes(fields, error);
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
        easydesk::client::signaling::encodeSessionAnswerEnvelope(
            fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        fail(QStringLiteral("E_ANSWER_SEND"),
             error.isEmpty()
                 ? QStringLiteral("Session answer could not be sent.")
                 : error);
        return;
    }

    m_timeout.stop();
    m_signedLocalDescriptionPayload = signingBytes;
    m_localDescriptionSequence = fields.route.sequence;
    if (!m_terminationSender.noteLocalSequence(
            m_localDescriptionSequence, error)) {
        fail(QStringLiteral("E_LOCAL_SEQUENCE"), error);
        return;
    }
    if (!m_iceSender.bindLocalDescription(
            m_signedLocalDescriptionPayload,
            m_localDescriptionSequence,
            sdp.toUtf8(),
            error)) {
        fail(QStringLiteral("E_ICE_BINDING"), error);
        return;
    }
    setState(State::AwaitingIce);
    if (m_transportState.state() != SessionTransportState::State::Connected)
        m_timeout.start(m_transportEverConnected ? kRecoveryTimeoutMilliseconds
                                                 : kIceConnectionTimeoutMilliseconds);
    emit localAnswerSent();
    emit descriptionsExchanged();
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
    if (!m_inputTransport.start(*m_mediaSession, m_context.capabilities)) {
        error = QStringLiteral("Input transport could not be started.");
        m_mediaSession.reset();
        return false;
    }
    if (!m_transportState.start(1)) {
        error = QStringLiteral("Media transport state could not be started.");
        m_inputTransport.stop();
        m_mediaSession.reset();
        return false;
    }
    if (!m_iceReceiver.begin(m_context, *m_mediaSession, 1, error)) {
        m_inputTransport.stop();
        m_transportState.stop();
        m_mediaSession.reset();
        return false;
    }
    if (!m_eventPump.start(*m_mediaSession)) {
        error = QStringLiteral("Media event pump could not be started.");
        m_transportState.stop();
        m_inputTransport.stop();
        m_iceReceiver.stop();
        m_mediaSession.reset();
        return false;
    }
    if (m_context.offerer && !m_mediaSession->createOffer()) {
        error = m_mediaSession->errorString();
        m_videoMonitor.stop();
        m_eventPump.stop();
        m_transportState.stop();
        m_iceReceiver.stop();
        m_inputTransport.stop();
        m_mediaSession.reset();
        return false;
    }
    error.clear();
    return true;
}

void SessionNegotiationCoordinator::closeMediaSession(bool keepTerminationReceiver) {
    m_timeout.stop();
    m_readyTimeout.stop();
    m_authorizationDeadline.stop();
    m_readyExchange.stop();
    m_videoMonitor.stop();
    m_eventPump.stop();
    m_inputTransport.stop();
    m_transportState.stop();
    m_iceSender.stop();
    m_iceReceiver.stop();
    if (!keepTerminationReceiver)
        m_terminationReceiver.stop();
    m_mediaSession.reset();
    m_videoRendererWindow = 0;
    m_transportEverConnected = false;
    m_controlOpen = false;
    m_handshakeComplete = false;
    m_localCaptureGeometry = {};
    m_remoteCaptureGeometry = {};
    m_sentCaptureGeneration = 0;
    m_remoteCaptureGeneration = 0;
    m_signedLocalDescriptionPayload.clear();
    m_localDescriptionSequence = 0;
    m_signedRemoteDescriptionPayload.clear();
    m_remoteDescriptionSequence = 0;
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
