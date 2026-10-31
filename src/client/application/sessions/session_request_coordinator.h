#pragma once

#include "features/sessions/model/session_access.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

class DeviceProfileStore;
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
    };
    Q_ENUM(State)

    SessionRequestCoordinator(DeviceProfileStore &profile,
                              WindowsIdentityStore &identity,
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

    bool requestSession(const QString &targetDeviceId,
                        const QString &assistanceCode,
                        SessionMode mode);

  signals:
    void stateChanged(SessionRequestCoordinator::State state);
    void requestDelivered();
    void failed(const QString &code, const QString &message);

  private:
    void handleMessage(const QByteArray &message);
    void handleResolvedIdentity(const QByteArray &message);
    void handleDelivery(const QByteArray &message);
    void handleSignalingError(const QString &code, const QString &message);
    void handleTimeout();
    void setState(State state);
    void failRequest(const QString &code, const QString &message);
    void clearRequest();

    DeviceProfileStore &m_profile;
    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_timeout;
    State m_state = State::Idle;
    QString m_targetDeviceId;
    QByteArray m_targetPublicKey;
    QByteArray m_requestId;
    QByteArray m_sessionId;
    QByteArray m_signedRequestPayload;
    SessionMode m_mode = SessionMode::ViewOnly;
};
