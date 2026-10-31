#pragma once

#include "features/sessions/session_service.h"

class DeviceProfileStore;

class LocalSessionService final : public SessionService {
    Q_OBJECT
  public:
    explicit LocalSessionService(DeviceProfileStore &profile, QObject *parent = nullptr);
    LocalIdentity localIdentity() const override {
        return m_identity;
    }
    bool isAvailable() const override {
        return false;
    }
    QString statusText() const override;
    void refreshAssistanceCode() override;
    QString validateConnection(const QString &id, const QString &code) const override;
    void start(const QString &deviceId, const SessionOptions &options) override;
    void stop() override;
    SessionTelemetry telemetry() const override {
        return m_telemetry;
    }
    SessionOptions options() const override {
        return m_options;
    }
    void setFrameRate(int frameRate) override;
    void setQuality(const QString &quality) override;

  private:
    LocalIdentity m_identity;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
    QString generateAssistanceCode() const;
};
