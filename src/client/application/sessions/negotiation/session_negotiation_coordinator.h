#pragma once

#include "sessions/negotiation/ice/session_ice_signal_sender.h"
#include "sessions/negotiation/ice/session_ice_signal_receiver.h"
#include "sessions/negotiation/media/session_media_event_pump.h"
#include "sessions/negotiation/session_negotiation_context.h"
#include "sessions/negotiation/turn/session_turn_binding.h"

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

    bool begin(const SessionNegotiationContext &context);
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
    void setState(State state);
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    SessionMediaEventPump m_eventPump;
    SessionIceSignalSender m_iceSender;
    SessionIceSignalReceiver m_iceReceiver;
    State m_state = State::Idle;
    SessionNegotiationContext m_context;
    SessionTurnConfiguration m_turn;
    std::unique_ptr<MediaSession> m_mediaSession;
    QByteArray m_signedLocalDescriptionPayload;
    quint64 m_localDescriptionSequence = 0;
    QByteArray m_signedRemoteDescriptionPayload;
    quint64 m_remoteDescriptionSequence = 0;
};
