#pragma once
#include "features/sessions/session_service.h"
class DeviceService;

class MockSessionService final : public SessionService {
    Q_OBJECT
  public:
    explicit MockSessionService(DeviceService &devices, QObject *parent = nullptr);
    LocalIdentity localIdentity() const override {
        return m_identity;
    }
    void refreshAssistanceCode() override;
    QString validateConnection(const QString &, const QString &) const override;
    void start(const QString &, const SessionOptions &) override;
    void stop() override;
    SessionTelemetry telemetry() const override {
        return m_telemetry;
    }
    SessionOptions options() const override {
        return m_options;
    }
    void setFrameRate(int) override;
    void setQuality(const QString &) override;

  private:
    DeviceService &m_devices;
    LocalIdentity m_identity;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
};
