#pragma once

#include "features/sessions/session_service.h"

class QSettings;

class LocalSessionService final : public SessionService {
    Q_OBJECT
  public:
    explicit LocalSessionService(QSettings &settings, QObject *parent = nullptr);
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
    QSettings &m_settings;
    LocalIdentity m_identity;
    SessionOptions m_options;
    SessionTelemetry m_telemetry;
    QString generateDigits(int digits) const;
};
