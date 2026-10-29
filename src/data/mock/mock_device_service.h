#pragma once
#include <functional>
#include "features/devices/device_service.h"

class MockDeviceService final : public DeviceService {
    Q_OBJECT
  public:
    explicit MockDeviceService(QObject *parent = nullptr);
    const QVector<Device> &devices() const override {
        return m_devices;
    }
    quint64 add(const QString &, const QString &, const QString &) override;
    quint64 rename(const QString &, const QString &) override;
    quint64 remove(const QString &) override;
    quint64 toggleFavorite(const QString &) override;

  private:
    QVector<Device> m_devices;
    quint64 m_nextRequest = 0;
    quint64 enqueue(std::function<QString()> operation);
};
