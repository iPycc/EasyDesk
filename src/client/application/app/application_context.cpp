#include "app/application_context.h"
#include "features/sessions/model/session_state.h"
#include "data/local/local_session_service.h"
#include "data/local/local_transfer_service.h"
#include "data/local/settings_device_service.h"
#include "data/local/windows_system_service.h"
#include "media/webrtc/media_runtime_library.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

ApplicationContext::ApplicationContext() {
    qRegisterMetaType<SessionTelemetry>();
    qRegisterMetaType<SessionOptions>();
    qRegisterMetaType<SessionState>();
    qRegisterMetaType<Device>();
    QString root = qEnvironmentVariable("EASYDESK_DATA_DIR");
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    const QString settingsPath = root + "/settings.ini";
    const QString legacyPath = root + "/preview.ini";
    if (!QFile::exists(settingsPath) && QFile::exists(legacyPath))
        QFile::copy(legacyPath, settingsPath);
    m_settings = std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
    m_devices = std::make_unique<SettingsDeviceService>(*m_settings);
    m_sessions = std::make_unique<LocalSessionService>(*m_settings);
    m_transfers = std::make_unique<LocalTransferService>();
    m_local = std::make_unique<WindowsSystemService>(*m_settings);
    m_media = std::make_unique<MediaRuntimeLibrary>();
    m_media->start();
}
ApplicationContext::~ApplicationContext() = default;
DeviceService &ApplicationContext::devices() const {
    return *m_devices;
}
SessionService &ApplicationContext::sessions() const {
    return *m_sessions;
}
TransferService &ApplicationContext::transfers() const {
    return *m_transfers;
}
QSettings &ApplicationContext::settings() const {
    return *m_settings;
}

LocalSystemService &ApplicationContext::localSystem() const {
    return *m_local;
}

MediaRuntimeLibrary &ApplicationContext::mediaRuntime() const {
    return *m_media;
}
