#pragma once
#include <QtWidgets>
#include "core/device.h"
#include "core/local_computer.h"
namespace Ui {
class DeviceList : public QScrollArea {
    Q_OBJECT
  public:
    explicit DeviceList(QWidget *parent = nullptr);
    void setDevices(const QVector<Device> &devices, const LocalComputerInfo &local, const QString &search,
                    bool favorites, bool onlineOnly);
    int visibleDeviceCount() const {
        return m_count;
    }
  signals:
    void deviceActivated(const QString &id);
    void detailsRequested(const QString &id);

  private:
    int m_count = 0;
    QMap<QString, bool> m_expanded;
};
} // namespace Ui
