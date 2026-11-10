#pragma once
#include <QObject>
#include "core/session.h"

class SessionService : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual LocalIdentity localIdentity() const = 0;
    virtual bool isAvailable() const = 0;
    virtual QString statusText() const = 0;
    virtual void refreshAssistanceCode() = 0;
    virtual QString validateConnection(const QString &id, const QString &code) const = 0;
    virtual void start(const QString &deviceId,
                       const QString &assistanceCode,
                       const SessionOptions &options) = 0;
    virtual void stop() = 0;
    virtual SessionTelemetry telemetry() const = 0;
    virtual SessionOptions options() const = 0;
    virtual void setFrameRate(int frameRate) = 0;
    virtual void setQuality(const QString &quality) = 0;
    virtual bool attachVideoTarget(quintptr nativeWindow) = 0;
    virtual void detachVideoTarget() = 0;
    virtual SessionInputStatus inputStatus() const = 0;
    virtual bool sendInput(InputStream stream, const InputEvent &event) = 0;
    virtual void releaseInput() = 0;
    virtual bool acceptIncomingSession(SessionMode grantedMode, bool rememberIdentity = false,
                                       bool grantLocalFileReceive = false) = 0;
    virtual bool confirmPeerIdentity(bool confirmed, bool rememberIdentity = false) = 0;
    virtual bool rejectIncomingSession() = 0;
  signals:
    void identityChanged();
    void availabilityChanged();
    void telemetryChanged(const SessionTelemetry &telemetry);
    void optionsChanged(const SessionOptions &options);
    void inputStatusChanged(const SessionInputStatus &status);
    void requestDelivered();
    void incomingSessionRequested(const IncomingSessionRequest &request);
    void peerIdentityConfirmationRequested(const PeerIdentityPrompt &prompt);
    void incomingSessionCancelled();
    void incomingSessionAccepted();
    void sessionAccepted();
    void sessionCancelled();
    void failed(const QString &message);
};
