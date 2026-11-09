#pragma once

#include "features/sessions/session_service.h"

class DeviceProfileStore;
class SignalingClient;
class SessionInvitationCoordinator;
class SessionNegotiationCoordinator;
class SessionRequestCoordinator;

class SignalingSessionService final : public SessionService {
    Q_OBJECT

  public:
    SignalingSessionService(DeviceProfileStore &profile,
                            SignalingClient &signaling,
                            SessionRequestCoordinator &requests,
                            SessionInvitationCoordinator &invitations,
                            SessionNegotiationCoordinator &negotiation,
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
    bool acceptIncomingSession(SessionMode grantedMode) override;
    bool rejectIncomingSession() override;

  private:
    DeviceProfileStore &m_profile;
    SignalingClient &m_signaling;
    SessionRequestCoordinator &m_requests;
    SessionInvitationCoordinator &m_invitations;
    SessionNegotiationCoordinator &m_negotiation;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
};
