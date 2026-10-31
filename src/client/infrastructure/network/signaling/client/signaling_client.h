#pragma once

#include "network/winhttp/winhttp_websocket_transport.h"

#include <QObject>
#include <QTimer>
#include <QUrl>

class DeviceProfileStore;
class WindowsIdentityStore;

class SignalingClient final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Offline,
        Connecting,
        Registering,
        Registered,
        RetryWaiting,
    };
    Q_ENUM(State)

    SignalingClient(DeviceProfileStore &profile,
                    WindowsIdentityStore &identity,
                    QObject *parent = nullptr);
    ~SignalingClient() override;

    State state() const;
    bool isRegistered() const;
    QString assistanceCode() const;
    qint64 assistanceCodeExpiresAt() const;

    bool start(const QUrl &endpoint, bool allowInsecureLoopback = false);
    void stop();
    bool refreshAssistanceCode();
    bool sendRegisteredMessage(const QByteArray &message);

  signals:
    void stateChanged(SignalingClient::State state);
    void profileChanged();
    void assistanceCodeChanged();
    void messageReceived(const QByteArray &message);
    void errorOccurred(const QString &code, const QString &message);

  private:
    void connectNow();
    void handleTransportState(WinHttpWebSocketTransport::State state);
    void handleMessage(const QByteArray &message);
    void handleChallenge(const QByteArray &message);
    void handleRegistrationResult(const QByteArray &message);
    void handleAssistanceCode(const QByteArray &message);
    void handleServerError(const QByteArray &message);
    void scheduleReconnect();
    void clearAssistanceCode();
    void setState(State state);
    void failProtocol(const QString &message);

    DeviceProfileStore &m_profile;
    WindowsIdentityStore &m_identity;
    WinHttpWebSocketTransport m_transport;
    QTimer m_reconnectTimer;
    QTimer m_assistanceExpiryTimer;
    QUrl m_endpoint;
    State m_state = State::Offline;
    QString m_assistanceCode;
    qint64 m_assistanceExpiresAt = 0;
    int m_retryAttempt = 0;
    int m_collisionRetries = 0;
    bool m_allowInsecureLoopback = false;
    bool m_shouldRun = false;
    bool m_retryImmediately = false;
};
