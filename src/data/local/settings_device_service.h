#pragma once

#include <functional>
#include "features/devices/device_service.h"

class QSettings;

class SettingsDeviceService final : public DeviceService {
    Q_OBJECT
  public:
    explicit SettingsDeviceService(QSettings &settings, QObject *parent = nullptr);
    const QVector<Device> &devices() const override {
        return m_devices;
    }
    quint64 add(const QString &id, const QString &name, const QString &system) override;
    quint64 rename(const QString &id, const QString &name) override;
    quint64 remove(const QString &id) override;
    quint64 toggleFavorite(const QString &id) override;

  private:
    QSettings &m_settings;
    QVector<Device> m_devices;
    quint64 m_nextRequest = 0;
    void load();
    void save();
    quint64 enqueue(std::function<QString()> operation);
};
