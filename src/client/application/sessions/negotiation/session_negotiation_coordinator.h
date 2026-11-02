#pragma once

#include "sessions/negotiation/ice/session_ice_signal_sender.h"
#include "sessions/negotiation/ice/session_ice_signal_receiver.h"
#include "sessions/negotiation/media/session_media_event_pump.h"
#include "sessions/negotiation/session_negotiation_context.h"
#include "sessions/negotiation/transport/session_transport_state.h"
#include "sessions/negotiation/turn/session_turn_binding.h"
#include "sessions/termination/session_termination_receiver.h"
#include "sessions/termination/session_termination_sender.h"

#include <QObject>
#include <QTimer>

#include <memory>

class MediaRuntimeLibrary;
class MediaSession;
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
    void handleMessage(const QByteArray &message);
    void handleTurnCredentials(const QByteArray &message);
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
    void closeMediaSession();
    void setState(State state);
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    SessionMediaEventPump m_eventPump;
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
    QByteArray m_signedLocalDescriptionPayload;
    quint64 m_localDescriptionSequence = 0;
    QByteArray m_signedRemoteDescriptionPayload;
    quint64 m_remoteDescriptionSequence = 0;
};
