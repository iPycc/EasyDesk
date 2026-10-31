#pragma once

#include "features/sessions/session_service.h"

class DeviceProfileStore;
class SignalingClient;
class SessionRequestCoordinator;

class SignalingSessionService final : public SessionService {
    Q_OBJECT

  public:
    SignalingSessionService(DeviceProfileStore &profile,
                            SignalingClient &signaling,
                            SessionRequestCoordinator &requests,
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

  private:
    DeviceProfileStore &m_profile;
    SignalingClient &m_signaling;
    SessionRequestCoordinator &m_requests;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
};
