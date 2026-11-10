#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class DeviceService;
class QSettings;

class TrustedPeerStore final : public QObject {
  public:
    enum class Status { NotAdded, Unverified, Matched, Changed, Corrupt, Invalid };

    TrustedPeerStore(QSettings &settings, DeviceService &devices, QObject *parent = nullptr);
    Status status(const QString &deviceId, const QByteArray &publicKey) const;
    bool remember(const QString &deviceId, const QByteArray &publicKey, QString &error);
    static QString fingerprint(const QByteArray &publicKey);

  private:
    void pruneRemovedDevices();
    QSettings &m_settings;
    DeviceService &m_devices;
};
