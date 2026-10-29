#include "data/local/local_session_service.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>

LocalSessionService::LocalSessionService(QSettings &settings, QObject *parent)
    : SessionService(parent), m_settings(settings) {
    m_identity.id = m_settings.value("identity/deviceId").toString();
    if (!QRegularExpression("^[0-9]{9}$").match(m_identity.id).hasMatch()) {
        m_identity.id = generateDigits(9);
        m_settings.setValue("identity/deviceId", m_identity.id);
    }
    m_identity.name = m_settings.value("identity/name", QStringLiteral("此设备")).toString();
    refreshAssistanceCode();
}

QString LocalSessionService::statusText() const {
    return QStringLiteral("信令未连接");
}

QString LocalSessionService::generateDigits(int digits) const {
    QString value;
    value.reserve(digits);
    auto *generator = QRandomGenerator::system();
    for (int index = 0; index < digits; ++index)
        value.append(QChar('0' + generator->bounded(10)));
    if (!value.isEmpty() && value.front() == '0')
        value[0] = QChar('1' + generator->bounded(9));
    return value;
}

void LocalSessionService::refreshAssistanceCode() {
    m_identity.assistanceCode = generateDigits(8);
    emit identityChanged();
}

QString LocalSessionService::validateConnection(const QString &id, const QString &code) const {
    if (!QRegularExpression("^[0-9]{9}$").match(id).hasMatch())
        return QStringLiteral("请输入 9 位数字设备 ID。");
    if (id == m_identity.id)
        return QStringLiteral("不能连接此设备。");
    if (!QRegularExpression("^[0-9]{8}$").match(code).hasMatch())
        return QStringLiteral("请输入 8 位数字临时协助码。");
    if (!isAvailable())
        return statusText();
    return {};
}

void LocalSessionService::start(const QString &, const SessionOptions &) {
    emit failed(statusText());
}

void LocalSessionService::stop() {}

void LocalSessionService::setFrameRate(int frameRate) {
    if (frameRate != 15 && frameRate != 30 && frameRate != 60)
        return;
    m_options.frameRate = frameRate;
    emit optionsChanged(m_options);
}

void LocalSessionService::setQuality(const QString &quality) {
    m_options.quality = quality;
    emit optionsChanged(m_options);
}
