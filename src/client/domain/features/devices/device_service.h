#pragma once
#include <QObject>
#include "core/device.h"

class DeviceService : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual const QVector<Device> &devices() const = 0;
    const Device *find(const QString &id) const;
    virtual quint64 add(const QString &id, const QString &name,
                        const QString &system = QStringLiteral("Windows 11")) = 0;
    virtual quint64 rename(const QString &id, const QString &name) = 0;
    virtual quint64 remove(const QString &id) = 0;
    virtual quint64 toggleFavorite(const QString &id) = 0;
  signals:
    void changed();
    void operationFinished(quint64 requestId, bool success, const QString &error);
};
