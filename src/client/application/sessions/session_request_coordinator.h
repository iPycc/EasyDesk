#pragma once

#include "features/sessions/model/session_access.h"
#include "network/signaling/protocol/session/session_request_codec.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

class DeviceProfileStore;
class MediaRuntimeLibrary;
class SignalingClient;
class WindowsIdentityStore;

class SessionRequestCoordinator final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        ResolvingIdentity,
        AwaitingDelivery,
        AwaitingDecision,
        AwaitingCancellationDelivery,
        Accepted,
    };
    Q_ENUM(State)

    SessionRequestCoordinator(DeviceProfileStore &profile,
                              WindowsIdentityStore &identity,
                              MediaRuntimeLibrary &crypto,
                              SignalingClient &signaling,
                              QObject *parent = nullptr);

    State state() const;
    bool isAvailable() const;
    QString targetDeviceId() const;
    QByteArray targetPublicKey() const;
    QByteArray requestId() const;
    QByteArray sessionId() const;
    QByteArray signedRequestPayload() const;
    SessionMode mode() const;
    SessionMode grantedMode() const;
    quint32 sessionEpoch() const;
    quint64 permissionEpoch() const;
    qint64 expiresAt() const;
    QByteArray acceptanceNonce() const;

    bool requestSession(const QString &targetDeviceId,
                        const QString &assistanceCode,
                        SessionMode mode);
    bool cancel();

  signals:
    void stateChanged(SessionRequestCoordinator::State state);
    void requestDelivered();
    void requestCancelled();
    void sessionAccepted();
    void sessionRejected(quint8 reason);
    void failed(const QString &code, const QString &message);

  private:
    void handleMessage(const QByteArray &message);
    void handleResolvedIdentity(const QByteArray &message);
    void handleDelivery(const QByteArray &message);
    void handleAcceptance(const QByteArray &message);
    void handleRejection(const QByteArray &message);
    void handleSignalingError(const QString &code, const QString &message);
    void handleTimeout();
    void setState(State state);
    void failRequest(const QString &code, const QString &message);
    void clearRequest();

    DeviceProfileStore &m_profile;
    WindowsIdentityStore &m_identity;
    MediaRuntimeLibrary &m_crypto;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    State m_state = State::Idle;
    QString m_targetDeviceId;
    QByteArray m_targetPublicKey;
    QByteArray m_requestId;
    QByteArray m_sessionId;
    QByteArray m_signedRequestPayload;
    easydesk::client::signaling::SessionRequestFields m_request;
    SessionMode m_mode = SessionMode::ViewOnly;
    SessionMode m_grantedMode = SessionMode::ViewOnly;
    quint32 m_sessionEpoch = 0;
    quint64 m_permissionEpoch = 0;
    qint64 m_expiresAt = 0;
    QByteArray m_acceptanceNonce;
};
