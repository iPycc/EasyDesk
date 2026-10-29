#pragma once
#include <QString>
#include <QVector>
#include <QMetaType>

enum class DeviceCategory { Computer, Mobile };
struct Device {
    QString id, name, system, location, lastSeen;
    bool online = false, favorite = false;
    DeviceCategory category = DeviceCategory::Computer;
};
Q_DECLARE_METATYPE(Device)

QString spacedId(const QString &id);
