#include "data/local/settings_device_service.h"

#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

SettingsDeviceService::SettingsDeviceService(QSettings &settings, QObject *parent)
    : DeviceService(parent), m_settings(settings) {
    load();
}

void SettingsDeviceService::load() {
    m_devices.clear();
    const int count = m_settings.beginReadArray("devices");
    for (int index = 0; index < count; ++index) {
        m_settings.setArrayIndex(index);
        Device device;
        device.id = m_settings.value("id").toString();
        device.name = m_settings.value("name").toString();
        device.system = m_settings.value("system", QStringLiteral("Windows 11")).toString();
        device.location = m_settings.value("location").toString();
        device.lastSeen = m_settings.value("lastSeen", QStringLiteral("从未连接")).toString();
        device.favorite = m_settings.value("favorite", false).toBool();
        device.online = false;
        device.category = static_cast<DeviceCategory>(m_settings.value("category", 0).toInt());
        if (QRegularExpression("^[0-9]{9}$").match(device.id).hasMatch() && !device.name.isEmpty())
            m_devices.push_back(device);
    }
    m_settings.endArray();
}

void SettingsDeviceService::save() {
    m_settings.beginWriteArray("devices", m_devices.size());
    for (int index = 0; index < m_devices.size(); ++index) {
        m_settings.setArrayIndex(index);
        const auto &device = m_devices[index];
        m_settings.setValue("id", device.id);
        m_settings.setValue("name", device.name);
        m_settings.setValue("system", device.system);
        m_settings.setValue("location", device.location);
        m_settings.setValue("lastSeen", device.lastSeen);
        m_settings.setValue("favorite", device.favorite);
        m_settings.setValue("category", static_cast<int>(device.category));
    }
    m_settings.endArray();
    m_settings.sync();
}

quint64 SettingsDeviceService::enqueue(std::function<QString()> operation) {
    const quint64 request = ++m_nextRequest;
    QTimer::singleShot(0, this, [this, request, operation = std::move(operation)] {
        const QString error = operation();
        if (error.isEmpty()) {
            save();
            emit changed();
        }
        emit operationFinished(request, error.isEmpty(), error);
    });
    return request;
}

quint64 SettingsDeviceService::add(const QString &id, const QString &name, const QString &system) {
    return enqueue([this, id, name, system] {
        if (!QRegularExpression("^[0-9]{9}$").match(id).hasMatch())
            return QStringLiteral("设备 ID 需要 9 位数字");
        if (name.trimmed().isEmpty())
            return QStringLiteral("请填写设备名称");
        if (find(id))
            return QStringLiteral("该设备 ID 已存在");
        m_devices.push_back({id, name.trimmed().left(40), system, {}, QStringLiteral("从未连接"),
                             false, false});
        return QString();
    });
}

quint64 SettingsDeviceService::rename(const QString &id, const QString &name) {
    return enqueue([this, id, name] {
        if (name.trimmed().isEmpty())
            return QStringLiteral("请填写设备名称");
        for (auto &device : m_devices) {
            if (device.id == id) {
                device.name = name.trimmed().left(40);
                return QString();
            }
        }
        return QStringLiteral("设备不存在");
    });
}

quint64 SettingsDeviceService::remove(const QString &id) {
    return enqueue([this, id] {
        for (int index = 0; index < m_devices.size(); ++index) {
            if (m_devices[index].id == id) {
                m_devices.removeAt(index);
                return QString();
            }
        }
        return QStringLiteral("设备不存在");
    });
}

quint64 SettingsDeviceService::toggleFavorite(const QString &id) {
    return enqueue([this, id] {
        for (auto &device : m_devices) {
            if (device.id == id) {
                device.favorite = !device.favorite;
                return QString();
            }
        }
        return QStringLiteral("设备不存在");
    });
}
