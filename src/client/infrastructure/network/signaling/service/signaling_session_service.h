#pragma once

#include "features/sessions/session_service.h"

class DeviceProfileStore;
class SignalingClient;
class SessionInvitationCoordinator;
class SessionNegotiationCoordinator;
class SessionRequestCoordinator;
class SessionInputCoordinator;
class WindowsIdentityStore;
class TrustedPeerStore;

class SignalingSessionService final : public SessionService {
    Q_OBJECT

  public:
    SignalingSessionService(DeviceProfileStore &profile,
                            SignalingClient &signaling,
                            SessionRequestCoordinator &requests,
                            SessionInvitationCoordinator &invitations,
                            SessionNegotiationCoordinator &negotiation,
                            SessionInputCoordinator &input,
                            WindowsIdentityStore &identity,
                            TrustedPeerStore &trustedPeers,
                            QObject *parent = nullptr);

    LocalIdentity localIdentity() const override;
    bool isAvailable() const override;
    QString statusText() const override;
    void refreshAssistanceCode() override;
    QString validateConnection(const QString &id, const QString &code) const override;
    void start(const QString &deviceId,
               const QString &assistanceCode,
               const SessionOptions &options) override;
    void stop() override;
    SessionTelemetry telemetry() const override;
    SessionOptions options() const override;
    void setFrameRate(int frameRate) override;
    void setQuality(const QString &quality) override;
    bool attachVideoTarget(quintptr nativeWindow) override;
    void detachVideoTarget() override;
    SessionInputStatus inputStatus() const override;
    bool sendInput(InputStream stream, const InputEvent &event) override;
    void releaseInput() override;
    bool acceptIncomingSession(SessionMode grantedMode, bool rememberIdentity = false) override;
    bool confirmPeerIdentity(bool confirmed, bool rememberIdentity = false) override;
    bool rejectIncomingSession() override;

  private:
    void publishInputStatus();
    DeviceProfileStore &m_profile;
    SignalingClient &m_signaling;
    SessionRequestCoordinator &m_requests;
    SessionInvitationCoordinator &m_invitations;
    SessionNegotiationCoordinator &m_negotiation;
    SessionInputCoordinator &m_input;
    WindowsIdentityStore &m_identity;
    TrustedPeerStore &m_trustedPeers;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
    SessionInputStatus m_lastInputStatus;
};
