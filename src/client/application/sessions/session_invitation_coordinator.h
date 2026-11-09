#pragma once

#include "sessions/negotiation/session_negotiation_context.h"
#include "features/sessions/model/session_access.h"
#include "network/signaling/protocol/session/session_decision_codec.h"
#include "network/signaling/protocol/session/session_request_decoder.h"

#include <QObject>
#include <QTimer>

class DeviceProfileStore;
class MediaRuntimeLibrary;
class SignalingClient;
class WindowsIdentityStore;

struct IncomingSessionInvitation {
    QString fromDeviceId;
    SessionMode requestedMode = SessionMode::ViewOnly;
    bool requesterIsController = true;
    qint64 expiresAt = 0;
};

class SessionInvitationCoordinator final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        AwaitingConsent,
        AwaitingAcceptanceDelivery,
        AwaitingRejectionDelivery,
        Accepted,
    };
    Q_ENUM(State)

    SessionInvitationCoordinator(DeviceProfileStore &profile,
                                 WindowsIdentityStore &identity,
                                 MediaRuntimeLibrary &crypto,
                                 SignalingClient &signaling,
                                 QObject *parent = nullptr);

    State state() const;
    IncomingSessionInvitation invitation() const;
    QByteArray requestId() const;
    QByteArray sessionId() const;
    QByteArray requesterPublicKey() const;
    QByteArray signedRequestPayload() const;
    quint32 sessionEpoch() const;
    SessionMode grantedMode() const;
    SessionNegotiationContext negotiationContext() const;

    bool accept(SessionMode grantedMode);
    bool reject();
    void completeSession();

  signals:
    void stateChanged(SessionInvitationCoordinator::State state);
    void invitationReceived(const IncomingSessionInvitation &invitation);
    void consentExpired();
    void sessionAccepted();
    void invitationRejected();
    void invitationCancelled(quint8 reason);
    void failed(const QString &code, const QString &message);

  private:
    void handleMessage(const QByteArray &message);
    void handleRequest(const QByteArray &message);
    void handleDelivery(const QByteArray &message);
    void handleCancellation(const QByteArray &message);
    void handleSignalingError(const QString &code, const QString &message);
    void handleTimeout();
    bool sendRejection(easydesk::client::signaling::SessionRejectReason reason);
    void setState(State state);
    void failInvitation(const QString &code, const QString &message);
    void clearInvitation();

    DeviceProfileStore &m_profile;
    WindowsIdentityStore &m_identity;
    MediaRuntimeLibrary &m_crypto;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    State m_state = State::Idle;
    easydesk::client::signaling::DecodedSessionRequest m_request;
    IncomingSessionInvitation m_invitation;
    quint32 m_sessionEpoch = 0;
    SessionMode m_grantedMode = SessionMode::ViewOnly;
    SessionNegotiationContext m_negotiation;
};
