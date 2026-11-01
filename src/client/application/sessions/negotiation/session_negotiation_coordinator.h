#pragma once

#include "sessions/negotiation/session_negotiation_context.h"
#include "sessions/negotiation/turn/session_turn_binding.h"

#include <QObject>
#include <QTimer>

#include <memory>

class MediaRuntimeLibrary;
class MediaSession;
class SignalingClient;

class SessionNegotiationCoordinator final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        AwaitingTurnCredentials,
        AwaitingLocalOffer,
        AwaitingRemoteOffer,
    };
    Q_ENUM(State)

    SessionNegotiationCoordinator(MediaRuntimeLibrary &media,
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
    void failed(const QString &code, const QString &message);

  private:
    void handleMessage(const QByteArray &message);
    void handleSignalingStateChanged();
    void handleTimeout();
    bool startMediaSession(QString &error);
    void setState(State state);
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    State m_state = State::Idle;
    SessionNegotiationContext m_context;
    SessionTurnConfiguration m_turn;
    std::unique_ptr<MediaSession> m_mediaSession;
};
