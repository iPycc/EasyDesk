#pragma once

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
    std::unique_ptr<MediaSession> createSession(const MediaSessionConfig &config);
    QString errorString() const;
    QString libraryPath() const;

  private:
    std::shared_ptr<MediaRuntimeState> m_state;
    QString m_libraryPath;
    QString m_error;
};
