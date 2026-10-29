#pragma once

#include "easydesk_media_abi.h"

#include <QString>

class MediaRuntimeLibrary final {
  public:
    MediaRuntimeLibrary() = default;
    ~MediaRuntimeLibrary();

    MediaRuntimeLibrary(const MediaRuntimeLibrary &) = delete;
    MediaRuntimeLibrary &operator=(const MediaRuntimeLibrary &) = delete;

    bool start(const QString &libraryPath = QString());
    void stop();

    bool isReady() const;
    QString errorString() const;
    QString libraryPath() const;

  private:
    using AbiVersionFunction = uint32_t (*)();
    using RuntimeCreateFunction = EasyDeskMediaStatus (*)(
        const EasyDeskMediaRuntimeConfig *, EasyDeskMediaRuntime **, EasyDeskMediaError *);
    using RuntimeIsReadyFunction = int32_t (*)(const EasyDeskMediaRuntime *);
    using RuntimeDestroyFunction = void (*)(EasyDeskMediaRuntime *);

    void *m_module = nullptr;
    EasyDeskMediaRuntime *m_runtime = nullptr;
    AbiVersionFunction m_abiVersion = nullptr;
    RuntimeCreateFunction m_create = nullptr;
    RuntimeIsReadyFunction m_isReady = nullptr;
    RuntimeDestroyFunction m_destroy = nullptr;
    QString m_libraryPath;
    QString m_error;
};
