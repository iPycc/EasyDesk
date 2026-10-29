#include "app/application_context.h"
#include "data/local/settings_device_service.h"
#include "data/mock/mock_session_service.h"
#include "data/mock/mock_transfer_service.h"
#include "data/local/windows_system_service.h"
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

ApplicationContext::ApplicationContext() {
    qRegisterMetaType<SessionTelemetry>();
    qRegisterMetaType<SessionOptions>();
    qRegisterMetaType<Device>();
    QString root = qEnvironmentVariable("EASYDESK_DATA_DIR");
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    m_settings = std::make_unique<QSettings>(root + "/preview.ini", QSettings::IniFormat);
    m_devices = std::make_unique<SettingsDeviceService>(*m_settings);
    m_sessions = std::make_unique<MockSessionService>(*m_devices);
    m_transfers = std::make_unique<MockTransferService>();
    m_local = std::make_unique<WindowsSystemService>(*m_settings);
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
