#include "data/settings/identity/device_profile_store.h"

#include <QRandomGenerator>
#include <QSettings>

namespace {

bool isDeviceId(const QString &value) {
    if (value.size() != 9)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

}  // namespace

DeviceProfileStore::DeviceProfileStore(QSettings &settings)
    : m_settings(settings) {}

bool DeviceProfileStore::initialize() {
    m_error.clear();
    const QString storedId = m_settings.value(QStringLiteral("identity/deviceId")).toString();
    const QString storedName = m_settings.value(QStringLiteral("identity/name")).toString();
    m_profile.deviceId = isDeviceId(storedId) ? storedId : generateDeviceId();
    m_profile.displayName = canonicalDisplayName(storedName);
    if (m_profile.displayName.isEmpty())
        m_profile.displayName = QStringLiteral("此设备");

    if (m_profile.deviceId != storedId || m_profile.displayName != storedName)
        return persist();
    return true;
}

bool DeviceProfileStore::rotateDeviceId() {
    if (!isDeviceId(m_profile.deviceId)) {
        m_error = QStringLiteral("Device profile is not initialized.");
        return false;
    }
    const QString previous = m_profile.deviceId;
    m_profile.deviceId = generateDeviceId(previous);
    if (persist())
        return true;
    m_profile.deviceId = previous;
    m_settings.setValue(QStringLiteral("identity/deviceId"), previous);
    m_settings.sync();
    return false;
}

const DeviceProfile &DeviceProfileStore::profile() const {
    return m_profile;
}

QString DeviceProfileStore::errorString() const {
    return m_error;
}

QString DeviceProfileStore::generateDeviceId(const QString &excluded) const {
    auto *generator = QRandomGenerator::system();
    QString value;
    do {
        value.clear();
        value.reserve(9);
        value.append(QChar(QLatin1Char('1').unicode() + generator->bounded(9)));
        for (int index = 1; index < 9; ++index)
            value.append(QChar(QLatin1Char('0').unicode() + generator->bounded(10)));
    } while (value == excluded);
    return value;
}

QString DeviceProfileStore::canonicalDisplayName(const QString &value) const {
    QString name = value.trimmed().normalized(QString::NormalizationForm_C).left(40);
    while (name.toUtf8().size() > 128)
        name.chop(1);
    if (!name.isEmpty() && name.at(name.size() - 1).isHighSurrogate())
        name.chop(1);
    return name;
}

bool DeviceProfileStore::persist() {
    m_settings.setValue(QStringLiteral("identity/deviceId"), m_profile.deviceId);
    m_settings.setValue(QStringLiteral("identity/name"), m_profile.displayName);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        m_error = QStringLiteral("Device profile could not be persisted.");
        return false;
    }
    m_error.clear();
    return true;
}
