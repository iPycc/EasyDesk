#include "media/webrtc/media_runtime_library.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <windows.h>

namespace {

QString windowsErrorMessage(DWORD errorCode) {
    wchar_t *message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const QString result = length > 0 && message != nullptr
                               ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
                               : QStringLiteral("Windows error %1").arg(errorCode);
    if (message != nullptr)
        LocalFree(message);
    return result;
}

template <typename Function>
Function resolveFunction(HMODULE module, const char *name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

}  // namespace

MediaRuntimeLibrary::~MediaRuntimeLibrary() {
    stop();
}

bool MediaRuntimeLibrary::start(const QString &libraryPath) {
    stop();
    m_error.clear();

    m_libraryPath = libraryPath.isEmpty()
                        ? QDir(QCoreApplication::applicationDirPath())
                              .absoluteFilePath(QStringLiteral("easydesk_media.dll"))
                        : QFileInfo(libraryPath).absoluteFilePath();

    const HMODULE module = LoadLibraryW(reinterpret_cast<const wchar_t *>(m_libraryPath.utf16()));
    if (module == nullptr) {
        m_error = QStringLiteral("Cannot load %1: %2")
                      .arg(QDir::toNativeSeparators(m_libraryPath),
                           windowsErrorMessage(GetLastError()));
        return false;
    }
    m_module = module;

    m_abiVersion = resolveFunction<AbiVersionFunction>(module, "easydesk_media_abi_version");
    m_create = resolveFunction<RuntimeCreateFunction>(module, "easydesk_media_runtime_create");
    m_isReady =
        resolveFunction<RuntimeIsReadyFunction>(module, "easydesk_media_runtime_is_ready");
    m_destroy =
        resolveFunction<RuntimeDestroyFunction>(module, "easydesk_media_runtime_destroy");
    if (m_abiVersion == nullptr || m_create == nullptr || m_isReady == nullptr ||
        m_destroy == nullptr) {
        m_error = QStringLiteral("Media runtime ABI exports are incomplete.");
        stop();
        return false;
    }

    const uint32_t actualAbiVersion = m_abiVersion();
    if (actualAbiVersion != EASYDESK_MEDIA_ABI_VERSION) {
        m_error = QStringLiteral("Media runtime ABI mismatch: expected %1, found %2.")
                      .arg(EASYDESK_MEDIA_ABI_VERSION)
                      .arg(actualAbiVersion);
        stop();
        return false;
    }

    EasyDeskMediaRuntimeConfig config{};
    config.size = sizeof(config);
    config.abi_version = EASYDESK_MEDIA_ABI_VERSION;
    EasyDeskMediaError error{};
    error.size = sizeof(error);
    const EasyDeskMediaStatus status = m_create(&config, &m_runtime, &error);
    if (status != EASYDESK_MEDIA_STATUS_OK || m_runtime == nullptr) {
        m_error = QString::fromUtf8(error.message).trimmed();
        if (m_error.isEmpty())
            m_error = QStringLiteral("Media runtime initialization failed with status %1.")
                          .arg(static_cast<int>(status));
        stop();
        return false;
    }
    if (m_isReady(m_runtime) != 1) {
        m_error = QStringLiteral("Media runtime did not become ready.");
        stop();
        return false;
    }
    return true;
}

void MediaRuntimeLibrary::stop() {
    if (m_runtime != nullptr && m_destroy != nullptr)
        m_destroy(m_runtime);
    m_runtime = nullptr;

    if (m_module != nullptr)
        FreeLibrary(static_cast<HMODULE>(m_module));
    m_module = nullptr;
    m_abiVersion = nullptr;
    m_create = nullptr;
    m_isReady = nullptr;
    m_destroy = nullptr;
}

bool MediaRuntimeLibrary::isReady() const {
    return m_runtime != nullptr && m_isReady != nullptr && m_isReady(m_runtime) == 1;
}

QString MediaRuntimeLibrary::errorString() const {
    return m_error;
}

QString MediaRuntimeLibrary::libraryPath() const {
    return m_libraryPath;
}
