#pragma once

#include "network/signaling/protocol/session/termination/session_end_codec.h"
#include "sessions/negotiation/session_negotiation_context.h"

#include <QObject>
#include <QTimer>

class SignalingClient;
class WindowsIdentityStore;

class SessionTerminationSender final : public QObject {
    Q_OBJECT

  public:
    SessionTerminationSender(WindowsIdentityStore &identity,
                             SignalingClient &signaling,
                             QObject *parent = nullptr);

    bool begin(const SessionNegotiationContext &context, QString &error);
    bool noteLocalSequence(quint64 sequence, QString &error);
    bool end(easydesk::client::signaling::SessionEndReason reason);
    void stop();
    bool isActive() const;
    bool isAwaitingDelivery() const;

    void handleDelivery(const QByteArray &message);

  signals:
    void sent(easydesk::client::signaling::SessionEndReason reason,
              quint64 sequence);
    void delivered(easydesk::client::signaling::SessionEndReason reason,
                   quint64 sequence);
    void failed(const QString &code, const QString &message);

  private:
    enum class State {
        Idle,
        Active,
        AwaitingDelivery,
    };

    void handleTimeout();
    void fail(const QString &code, const QString &message);

    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    SessionNegotiationContext m_context;
    State m_state = State::Idle;
    easydesk::client::signaling::SessionEndReason m_reason =
        easydesk::client::signaling::SessionEndReason::UserStopped;
    quint64 m_lastLocalSequence = 0;
};
