#pragma once

#include <QString>

class QSettings;

struct DeviceProfile {
    QString deviceId;
    QString displayName;
};

class DeviceProfileStore final {
  public:
    explicit DeviceProfileStore(QSettings &settings);

    bool initialize();
    bool rotateDeviceId();
    const DeviceProfile &profile() const;
    QString errorString() const;

  private:
    QString generateDeviceId(const QString &excluded = {}) const;
    QString canonicalDisplayName(const QString &value) const;
    bool persist();

    QSettings &m_settings;
    DeviceProfile m_profile;
    QString m_error;
};
