#pragma once

#include "sessions/negotiation/authorization/session_authorization_deadline.h"
#include "sessions/negotiation/control/session_ready_exchange.h"
#include "sessions/negotiation/control/session_capture_geometry_codec.h"
#include "sessions/negotiation/ice/session_ice_signal_sender.h"
#include "sessions/negotiation/ice/session_ice_signal_receiver.h"
#include "sessions/negotiation/media/session_media_event_pump.h"
#include "sessions/negotiation/media/session_video_monitor.h"
#include "sessions/negotiation/session_negotiation_context.h"
#include "sessions/negotiation/transport/session_transport_state.h"
#include "sessions/negotiation/turn/session_turn_binding.h"
#include "sessions/input/session_input_transport.h"
#include "sessions/files/session_file_transport.h"
#include "sessions/termination/session_termination_receiver.h"
#include "sessions/termination/session_termination_sender.h"

#include <QObject>
#include <QTimer>

#include <memory>

class MediaRuntimeLibrary;
class MediaSession;
class SessionFileIncoming;
class SessionFileOutgoing;
class SignalingClient;
class WindowsIdentityStore;

class SessionNegotiationCoordinator final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        AwaitingTurnCredentials,
        AwaitingLocalOffer,
        AwaitingRemoteOffer,
        ApplyingRemoteOffer,
        AwaitingLocalAnswer,
        AwaitingRemoteAnswer,
        ApplyingRemoteAnswer,
        AwaitingIce,
        Ending,
    };
    Q_ENUM(State)

    SessionNegotiationCoordinator(MediaRuntimeLibrary &media,
                                  WindowsIdentityStore &identity,
                                  SignalingClient &signaling,
                                  QObject *parent = nullptr);
    ~SessionNegotiationCoordinator() override;

    State state() const;
    SessionNegotiationContext context() const;
    SessionTurnConfiguration turnConfiguration() const;
    SessionTransportState::State transportState() const;
    bool hasPresentedFrame() const;
    QSize presentedFrameSize() const;
    QSize presentedTargetSize() const;
    RemoteCaptureGeometry remoteCaptureGeometry() const;
    bool isSessionReady() const;
    bool isInputChannelOpen(InputStream stream) const;
    SessionInputTransport::SendResult sendInputMessage(InputStream stream, const QByteArray &message);
    bool isFileChannelOpen(FileChannel channel) const;
    SessionFileTransport::SendResult sendFileMessage(FileChannel channel, const QByteArray &message);

    bool begin(const SessionNegotiationContext &context);
    bool endSession(easydesk::client::signaling::SessionEndReason reason);
    bool attachVideoRenderer(quintptr nativeWindow, QString &error);
    bool detachVideoRenderer(QString &error);
    void stop();

  signals:
    void stateChanged(SessionNegotiationCoordinator::State state);
    void relayReady();
    void mediaStarted();
    void localOfferSent();
    void localAnswerSent();
    void descriptionsExchanged();
    void remoteCandidatesApplied(int count, quint64 sequence);
    void remoteGatheringComplete(quint64 sequence);
    void transportStateChanged(SessionTransportState::State state);
    void transportConnected();
    void sessionReady();
    void inputAvailabilityChanged();
    void inputMessageReceived(InputStream stream, const QByteArray &message);
    void fileAvailabilityChanged();
    void fileMessageReceived(FileChannel channel, const QByteArray &message);
    void captureGeometryChanged(const QRect &region, quint64 generation, bool valid);
    void remoteCaptureGeometryChanged(const QSize &size, quint64 generation, bool valid);
    void firstFramePresented(quint32 width, quint32 height);
    void framePresented();
    void remoteSessionEnded(
        easydesk::client::signaling::SessionEndReason reason,
        quint64 sequence);
    void localSessionEndSent(
        easydesk::client::signaling::SessionEndReason reason,
        quint64 sequence);
    void localSessionEnded(
        easydesk::client::signaling::SessionEndReason reason,
        quint64 sequence);
    void failed(const QString &code, const QString &message);

  private:
    friend class SessionFileIncoming;
    friend class SessionFileOutgoing;
    bool setFileReceivingEnabled(bool enabled);
    bool setFileSendingEnabled(bool enabled);
    void handleMessage(const QByteArray &message);
    void handleTurnCredentials(const QByteArray &message);
    void handleTurnUnavailable(const QByteArray &message);
    void startConfiguredMedia(bool hasRelay);
    void handleControlOpen();
    void handleControlMessage(const QByteArray &message);
    void handleCaptureGeometry(const QByteArray &message);
    void publishCaptureGeometry();
    bool sendControlMessage(const QByteArray &message);
    void finishReadyExchange();
    bool activateVideoRenderer(QString &error);
    void handleRemoteOffer(const QByteArray &message);
    void handleRemoteAnswer(const QByteArray &message);
    void handleSignalingStateChanged();
    void handleSignalingError(const QString &code, const QString &message);
    void handleTimeout();
    void handleLocalDescription(quint32 iceGeneration,
                                quint32 sdpType,
                                const QByteArray &sdp);
    void handleRemoteDescriptionApplied(quint32 iceGeneration,
                                        quint32 sdpType);
    void sendLocalAnswer(const QString &sdp);
    bool startMediaSession(QString &error);
    void closeMediaSession(bool keepTerminationReceiver = false);
    void setState(State state);
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    QTimer m_readyTimeout;
    SessionAuthorizationDeadline m_authorizationDeadline;
    SessionReadyExchange m_readyExchange;
    SessionInputTransport m_inputTransport;
    SessionFileTransport m_fileTransport;
    SessionMediaEventPump m_eventPump;
    SessionVideoMonitor m_videoMonitor;
    SessionTransportState m_transportState;
    SessionIceSignalSender m_iceSender;
    SessionIceSignalReceiver m_iceReceiver;
    SessionTerminationReceiver m_terminationReceiver;
    SessionTerminationSender m_terminationSender;
    State m_state = State::Idle;
    SessionNegotiationContext m_context;
    SessionTurnConfiguration m_turn;
    std::unique_ptr<MediaSession> m_mediaSession;
    quintptr m_videoRendererWindow = 0;
    bool m_transportEverConnected = false;
    bool m_controlOpen = false;
    bool m_handshakeComplete = false;
    RemoteCaptureGeometry m_localCaptureGeometry;
    RemoteCaptureGeometry m_remoteCaptureGeometry;
    quint64 m_sentCaptureGeneration = 0;
    quint64 m_remoteCaptureGeneration = 0;
    QByteArray m_signedLocalDescriptionPayload;
    quint64 m_localDescriptionSequence = 0;
    QByteArray m_signedRemoteDescriptionPayload;
    quint64 m_remoteDescriptionSequence = 0;
};
