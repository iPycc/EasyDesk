#pragma once

#include <QByteArray>
#include <QString>

class MediaRuntimeLibrary;
class QSettings;

class WindowsIdentityStore final {
  public:
    WindowsIdentityStore(QSettings &settings, MediaRuntimeLibrary &crypto);
    ~WindowsIdentityStore();

    WindowsIdentityStore(const WindowsIdentityStore &) = delete;
    WindowsIdentityStore &operator=(const WindowsIdentityStore &) = delete;

    bool initialize();
    bool isReady() const;
    QByteArray publicKey() const;
    QByteArray sign(const QByteArray &message);
    QString errorString() const;

  private:
    QByteArray createSeed();
    QByteArray protectSeed(const QByteArray &seed);
    QByteArray unprotectSeed(const QByteArray &protectedSeed);
    void clearPrivateKey();

    QSettings &m_settings;
    MediaRuntimeLibrary &m_crypto;
    QByteArray m_publicKey;
    QByteArray m_privateKey;
    QString m_error;
};
