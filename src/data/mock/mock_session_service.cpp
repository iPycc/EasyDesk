#include "data/mock/mock_session_service.h"
#include "features/devices/device_service.h"
#include <QRandomGenerator>
#include <QRegularExpression>

MockSessionService::MockSessionService(DeviceService &devices, QObject *parent)
    : SessionService(parent), m_devices(devices) {
    m_identity = {"485921036", QStringLiteral("我的笔记本"), "37286041"};
    m_telemetry = {18, 30, "192.0.2.10", "198.51.100.24", "UDP", 0.2, true, true};
}
void MockSessionService::refreshAssistanceCode() {
    m_identity.assistanceCode = QString::number(QRandomGenerator::global()->bounded(10000000, 99999999));
    emit identityChanged();
}
QString MockSessionService::validateConnection(const QString &id, const QString &code) const {
    if (!QRegularExpression("^[0-9]{9}$").match(id).hasMatch())
        return QStringLiteral("请输入 9 位数字设备 ID。");
    if (id == m_identity.id)
        return QStringLiteral("请选择其他设备，不能连接此设备。");
    if (!QRegularExpression("^[0-9]{8}$").match(code).hasMatch())
        return QStringLiteral("请输入 8 位数字临时协助码。");
    const auto *device = m_devices.find(id);
    if (!device)
        return QStringLiteral("演示列表中没有此设备，请使用下方示例。");
    if (!device->online)
        return QStringLiteral("此演示设备已离线，请使用在线设备。");
    return {};
}
void MockSessionService::start(const QString &, const SessionOptions &options) {
    m_options = options;
    m_telemetry.frameRate = options.frameRate;
    emit optionsChanged(m_options);
    emit telemetryChanged(m_telemetry);
}
void MockSessionService::stop() {}
void MockSessionService::setFrameRate(int fps) {
    if (fps != 15 && fps != 30 && fps != 60)
        return;
    m_options.frameRate = fps;
    m_telemetry.frameRate = fps;
    emit optionsChanged(m_options);
    emit telemetryChanged(m_telemetry);
}
void MockSessionService::setQuality(const QString &quality) {
    m_options.quality = quality;
    emit optionsChanged(m_options);
}
