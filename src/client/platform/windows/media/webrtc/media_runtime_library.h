#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

class MediaSession;
struct MediaSessionConfig;
struct MediaRuntimeState;

class MediaRuntimeLibrary final {
  public:
    MediaRuntimeLibrary() = default;
    ~MediaRuntimeLibrary();

    MediaRuntimeLibrary(const MediaRuntimeLibrary &) = delete;
    MediaRuntimeLibrary &operator=(const MediaRuntimeLibrary &) = delete;

    bool start(const QString &libraryPath = QString());
    void stop();

    bool isReady() const;
    bool deriveEd25519KeyPair(const QByteArray &seed,
                              QByteArray &publicKey,
                              QByteArray &privateKey);
    QByteArray signEd25519(const QByteArray &privateKey, const QByteArray &message);
    bool verifyEd25519(const QByteArray &publicKey,
                       const QByteArray &message,
                       const QByteArray &signature);
    std::unique_ptr<MediaSession> createSession(const MediaSessionConfig &config);
    QString errorString() const;
    QString libraryPath() const;

  private:
    std::shared_ptr<MediaRuntimeState> m_state;
    QString m_libraryPath;
    QString m_error;
};
