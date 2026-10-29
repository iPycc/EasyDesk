#include "data/mock/mock_device_service.h"
#include <QRegularExpression>
#include <QTimer>

MockDeviceService::MockDeviceService(QObject *parent) : DeviceService(parent) {
    m_devices = {{"812536904", QStringLiteral("设计工作站"), "Windows 11", QStringLiteral("工作室"),
                  QStringLiteral("刚刚"), true, true},
                 {"603218795", QStringLiteral("办公笔记本"), "macOS", QStringLiteral("办公室"),
                  QStringLiteral("12 分钟前"), true, true},
                 {"295741860", QStringLiteral("客厅电脑"), "Windows 10", QStringLiteral("家"),
                  QStringLiteral("昨天 21:36"), false, false},
                 {"759164203", QStringLiteral("剪辑工作站"), "Ubuntu Linux", QStringLiteral("工作室"),
                  QStringLiteral("今天 09:42"), true, false}};
}
quint64 MockDeviceService::enqueue(std::function<QString()> operation) {
    const quint64 request = ++m_nextRequest;
    QTimer::singleShot(0, this, [this, request, operation] {
        const QString error = operation();
        if (error.isEmpty())
            emit changed();
        emit operationFinished(request, error.isEmpty(), error);
    });
    return request;
}
quint64 MockDeviceService::add(const QString &id, const QString &name, const QString &system) {
    return enqueue([this, id, name, system] {
        if (!QRegularExpression("^[0-9]{9}$").match(id).hasMatch())
            return QStringLiteral("设备 ID 需要 9 位数字");
        if (name.trimmed().isEmpty())
            return QStringLiteral("请填写设备名称");
        if (find(id))
            return QStringLiteral("该设备 ID 已存在");
        m_devices.push_back({id, name.trimmed().left(40), system, QStringLiteral("未分组"),
                             QStringLiteral("从未连接"), false, false});
        return QString();
    });
}
quint64 MockDeviceService::rename(const QString &id, const QString &name) {
    return enqueue([this, id, name] {
        if (name.trimmed().isEmpty())
            return QStringLiteral("请填写设备名称");
        for (auto &device : m_devices)
            if (device.id == id) {
                device.name = name.trimmed().left(40);
                return QString();
            }
        return QStringLiteral("设备不存在");
    });
}
quint64 MockDeviceService::remove(const QString &id) {
    return enqueue([this, id] {
        for (int i = 0; i < m_devices.size(); ++i)
            if (m_devices[i].id == id) {
                m_devices.removeAt(i);
                return QString();
            }
        return QStringLiteral("设备不存在");
    });
}
quint64 MockDeviceService::toggleFavorite(const QString &id) {
    return enqueue([this, id] {
        for (auto &device : m_devices)
            if (device.id == id) {
                device.favorite = !device.favorite;
                return QString();
            }
        return QStringLiteral("设备不存在");
    });
}
