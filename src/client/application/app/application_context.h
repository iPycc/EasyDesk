#pragma once
#include <QObject>
#include <memory>
class DeviceService;
class SessionService;
class TransferService;
class QSettings;
class LocalSystemService;
class MediaRuntimeLibrary;

class ApplicationContext final : public QObject {
  public:
    ApplicationContext();
    ~ApplicationContext() override;
    DeviceService &devices() const;
    SessionService &sessions() const;
    TransferService &transfers() const;
    QSettings &settings() const;
    LocalSystemService &localSystem() const;
    MediaRuntimeLibrary &mediaRuntime() const;

  private:
    std::unique_ptr<DeviceService> m_devices;
    std::unique_ptr<SessionService> m_sessions;
    std::unique_ptr<TransferService> m_transfers;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<LocalSystemService> m_local;
    std::unique_ptr<MediaRuntimeLibrary> m_media;
};
