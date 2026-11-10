#include "data/settings/identity/trusted_peer_store.h"

#include "features/devices/device_service.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSettings>

namespace {
bool validId(const QString &id) {
    return QRegularExpression(QStringLiteral("^[0-9]{9}$")).match(id).hasMatch();
}
QString settingsKey(const QString &id) { return QStringLiteral("trustedPeers/") + id; }
} // namespace

TrustedPeerStore::TrustedPeerStore(QSettings &settings, DeviceService &devices, QObject *parent)
    : QObject(parent), m_settings(settings), m_devices(devices) {
    connect(&m_devices, &DeviceService::changed, this, [this] { pruneRemovedDevices(); });
    pruneRemovedDevices();
}

TrustedPeerStore::Status TrustedPeerStore::status(const QString &deviceId, const QByteArray &publicKey) const {
    if (!validId(deviceId) || publicKey.size() != 32 || publicKey == QByteArray(32, '\0'))
        return Status::Invalid;
    if (!m_devices.find(deviceId))
        return Status::NotAdded;
    if (m_settings.status() != QSettings::NoError)
        return Status::Corrupt;
    const QString key = settingsKey(deviceId);
    if (!m_settings.contains(key))
        return Status::Unverified;
    const QString stored = m_settings.value(key).toString();
    if (!QRegularExpression(QStringLiteral("^[0-9a-f]{64}$")).match(stored).hasMatch())
        return Status::Corrupt;
    return QByteArray::fromHex(stored.toLatin1()) == publicKey ? Status::Matched : Status::Changed;
}

bool TrustedPeerStore::remember(const QString &deviceId, const QByteArray &publicKey, QString &error) {
    const auto current = status(deviceId, publicKey);
    if (current == Status::Matched) { error.clear(); return true; }
    if (current != Status::Unverified || m_settings.status() != QSettings::NoError) {
        error = QStringLiteral("Peer identity cannot be saved or has changed.");
        return false;
    }
    const QString key = settingsKey(deviceId);
    m_settings.setValue(key, QString::fromLatin1(publicKey.toHex()));
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        m_settings.remove(key);
        m_settings.sync();
        error = QStringLiteral("Peer identity could not be saved.");
        return false;
    }
    error.clear();
    return true;
}

QString TrustedPeerStore::fingerprint(const QByteArray &publicKey) {
    if (publicKey.size() != 32 || publicKey == QByteArray(32, '\0'))
        return {};
    const QString hex = QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex()).toUpper();
    QString result;
    for (int i = 0; i < hex.size(); i += 4) {
        if (!result.isEmpty()) result.append(QLatin1Char(' '));
        result.append(hex.mid(i, 4));
    }
    return result;
}

void TrustedPeerStore::pruneRemovedDevices() {
    if (m_settings.status() != QSettings::NoError) return;
    m_settings.beginGroup(QStringLiteral("trustedPeers"));
    const QStringList ids = m_settings.childKeys();
    bool changed = false;
    for (const QString &id : ids) {
        if (!m_devices.find(id)) { m_settings.remove(id); changed = true; }
    }
    m_settings.endGroup();
    if (changed) m_settings.sync();
}
