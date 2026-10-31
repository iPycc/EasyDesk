#include "media/webrtc/media_runtime_library.h"

#include "media/webrtc/media_runtime_state.h"
#include "media/webrtc/media_session.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstring>
#include <utility>
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

uint64_t capabilityMask(SessionCapabilities capabilities) {
    uint64_t mask = 0;
    if (capabilities.testFlag(SessionCapability::ViewDesktop))
        mask |= EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN;
    if (capabilities.testFlag(SessionCapability::ControlInput))
        mask |= EASYDESK_MEDIA_CAPABILITY_SEND_INPUT;
    if (capabilities.testFlag(SessionCapability::TransferFiles))
        mask |= EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES;
    return mask;
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

    auto state = std::make_shared<MediaRuntimeState>();
    const HMODULE module = LoadLibraryW(reinterpret_cast<const wchar_t *>(m_libraryPath.utf16()));
    if (module == nullptr) {
        m_error = QStringLiteral("Cannot load %1: %2")
                      .arg(QDir::toNativeSeparators(m_libraryPath),
                           windowsErrorMessage(GetLastError()));
        return false;
    }
    state->module = module;

    MediaApi &api = state->api;
    api.abiVersion = resolveFunction<MediaApi::AbiVersionFunction>(
        module, "easydesk_media_abi_version");
    api.identityKeyPair = resolveFunction<MediaApi::IdentityKeyPairFunction>(
        module, "easydesk_identity_ed25519_keypair_from_seed");
    api.identitySign = resolveFunction<MediaApi::IdentitySignFunction>(
        module, "easydesk_identity_ed25519_sign");
    api.identityVerify = resolveFunction<MediaApi::IdentityVerifyFunction>(
        module, "easydesk_identity_ed25519_verify");
    api.runtimeCreate = resolveFunction<MediaApi::RuntimeCreateFunction>(
        module, "easydesk_media_runtime_create");
    api.runtimeIsReady = resolveFunction<MediaApi::RuntimeIsReadyFunction>(
        module, "easydesk_media_runtime_is_ready");
    api.runtimeDestroy = resolveFunction<MediaApi::RuntimeDestroyFunction>(
        module, "easydesk_media_runtime_destroy");
    api.sessionCreate = resolveFunction<MediaApi::SessionCreateFunction>(
        module, "easydesk_media_session_create");
    api.sessionCreateOffer = resolveFunction<MediaApi::SessionActionFunction>(
        module, "easydesk_media_session_create_offer");
    api.sessionCreateAnswer = resolveFunction<MediaApi::SessionActionFunction>(
        module, "easydesk_media_session_create_answer");
    api.sessionSetRemoteDescription =
        resolveFunction<MediaApi::SessionSetRemoteDescriptionFunction>(
            module, "easydesk_media_session_set_remote_description");
    api.sessionAddIceCandidate = resolveFunction<MediaApi::SessionAddIceCandidateFunction>(
        module, "easydesk_media_session_add_ice_candidate");
    api.sessionSetPermissions = resolveFunction<MediaApi::SessionSetPermissionsFunction>(
        module, "easydesk_media_session_set_permissions");
    api.sessionPollEvent = resolveFunction<MediaApi::SessionPollEventFunction>(
        module, "easydesk_media_session_poll_event");
    api.sessionClose = resolveFunction<MediaApi::SessionActionFunction>(
        module, "easydesk_media_session_close");
    api.sessionDestroy = resolveFunction<MediaApi::SessionDestroyFunction>(
        module, "easydesk_media_session_destroy");
    if (!api.complete()) {
        m_error = QStringLiteral("Media runtime ABI exports are incomplete.");
        return false;
    }

    const uint32_t actualAbiVersion = api.abiVersion();
    if (actualAbiVersion != EASYDESK_MEDIA_ABI_VERSION) {
        m_error = QStringLiteral("Media runtime ABI mismatch: expected %1, found %2.")
                      .arg(EASYDESK_MEDIA_ABI_VERSION)
                      .arg(actualAbiVersion);
        return false;
    }

    EasyDeskMediaRuntimeConfig config{};
    config.size = sizeof(config);
    config.abi_version = EASYDESK_MEDIA_ABI_VERSION;
    EasyDeskMediaError error{};
    error.size = sizeof(error);
    const EasyDeskMediaStatus status = api.runtimeCreate(&config, &state->runtime, &error);
    if (status != EASYDESK_MEDIA_STATUS_OK || state->runtime == nullptr) {
        m_error = QString::fromUtf8(error.message).trimmed();
        if (m_error.isEmpty())
            m_error = QStringLiteral("Media runtime initialization failed with status %1.")
                          .arg(static_cast<int>(status));
        return false;
    }
    if (api.runtimeIsReady(state->runtime) != 1) {
        m_error = QStringLiteral("Media runtime did not become ready.");
        return false;
    }

    m_state = std::move(state);
    return true;
}

void MediaRuntimeLibrary::stop() {
    m_state.reset();
}

bool MediaRuntimeLibrary::isReady() const {
    return m_state && m_state->runtime &&
           m_state->api.runtimeIsReady(m_state->runtime) == 1;
}

bool MediaRuntimeLibrary::deriveEd25519KeyPair(const QByteArray &seed,
                                               QByteArray &publicKey,
                                               QByteArray &privateKey) {
    publicKey.clear();
    privateKey.clear();
    if (!isReady() || seed.size() != EASYDESK_IDENTITY_ED25519_SEED_SIZE) {
        m_error = QStringLiteral("Ed25519 seed must contain exactly 32 bytes.");
        return false;
    }

    publicKey.resize(EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE);
    privateKey.resize(EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE);
    EasyDeskMediaError error{};
    error.size = sizeof(error);
    const auto status = m_state->api.identityKeyPair(
        reinterpret_cast<const uint8_t *>(seed.constData()),
        reinterpret_cast<uint8_t *>(publicKey.data()),
        reinterpret_cast<uint8_t *>(privateKey.data()), &error);
    if (status != EASYDESK_MEDIA_STATUS_OK) {
        publicKey.clear();
        privateKey.fill('\0');
        privateKey.clear();
        m_error = QString::fromUtf8(error.message).trimmed();
        return false;
    }
    m_error.clear();
    return true;
}

QByteArray MediaRuntimeLibrary::signEd25519(const QByteArray &privateKey,
                                            const QByteArray &message) {
    if (!isReady() || privateKey.size() != EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE) {
        m_error = QStringLiteral("Ed25519 private key must contain exactly 64 bytes.");
        return {};
    }

    QByteArray signature(EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE, '\0');
    EasyDeskMediaError error{};
    error.size = sizeof(error);
    const auto status = m_state->api.identitySign(
        reinterpret_cast<const uint8_t *>(privateKey.constData()),
        reinterpret_cast<const uint8_t *>(message.constData()),
        static_cast<uint32_t>(message.size()),
        reinterpret_cast<uint8_t *>(signature.data()), &error);
    if (status != EASYDESK_MEDIA_STATUS_OK) {
        signature.fill('\0');
        signature.clear();
        m_error = QString::fromUtf8(error.message).trimmed();
        return {};
    }
    m_error.clear();
    return signature;
}

bool MediaRuntimeLibrary::verifyEd25519(const QByteArray &publicKey,
                                        const QByteArray &message,
                                        const QByteArray &signature) {
    if (!isReady() || publicKey.size() != EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE ||
        signature.size() != EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE) {
        m_error = QStringLiteral("Ed25519 verification key or signature length is invalid.");
        return false;
    }

    EasyDeskMediaError error{};
    error.size = sizeof(error);
    const auto status = m_state->api.identityVerify(
        reinterpret_cast<const uint8_t *>(publicKey.constData()),
        reinterpret_cast<const uint8_t *>(message.constData()),
        static_cast<uint32_t>(message.size()),
        reinterpret_cast<const uint8_t *>(signature.constData()), &error);
    if (status != EASYDESK_MEDIA_STATUS_OK) {
        m_error = QString::fromUtf8(error.message).trimmed();
        return false;
    }
    m_error.clear();
    return true;
}

std::unique_ptr<MediaSession>
MediaRuntimeLibrary::createSession(const MediaSessionConfig &config) {
    if (!isReady()) {
        m_error = QStringLiteral("Media runtime is not ready.");
        return nullptr;
    }
    if (config.sessionId.size() != EASYDESK_MEDIA_SESSION_ID_SIZE) {
        m_error = QStringLiteral("Media session ID must contain exactly 16 bytes.");
        return nullptr;
    }

    QVector<QByteArray> urls;
    QVector<QByteArray> usernames;
    QVector<QByteArray> credentials;
    urls.reserve(config.iceServers.size());
    usernames.reserve(config.iceServers.size());
    credentials.reserve(config.iceServers.size());
    for (const MediaIceServer &server : config.iceServers) {
        urls.push_back(server.url.toUtf8());
        usernames.push_back(server.username.toUtf8());
        credentials.push_back(server.credential.toUtf8());
    }

    QVector<EasyDeskMediaIceServerConfig> nativeIceServers;
    nativeIceServers.reserve(config.iceServers.size());
    for (int index = 0; index < config.iceServers.size(); ++index) {
        EasyDeskMediaIceServerConfig server{};
        server.size = sizeof(server);
        server.url_utf8 = urls[index].constData();
        server.url_size = static_cast<uint32_t>(urls[index].size());
        server.username_utf8 = usernames[index].constData();
        server.username_size = static_cast<uint32_t>(usernames[index].size());
        server.credential_utf8 = credentials[index].constData();
        server.credential_size = static_cast<uint32_t>(credentials[index].size());
        nativeIceServers.push_back(server);
    }

    EasyDeskMediaSessionConfig native{};
    native.size = sizeof(native);
    native.abi_version = EASYDESK_MEDIA_ABI_VERSION;
    std::memcpy(native.session_id, config.sessionId.constData(), EASYDESK_MEDIA_SESSION_ID_SIZE);
    native.session_epoch = config.sessionEpoch;
    native.permission_epoch = config.permissionEpoch;
    native.capabilities = capabilityMask(config.capabilities);
    native.role = config.role == SessionRole::Controller
                      ? EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER
                      : EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED;
    native.ice_generation = config.iceGeneration;
    native.ice_servers = nativeIceServers.isEmpty() ? nullptr : nativeIceServers.constData();
    native.ice_server_count = static_cast<uint32_t>(nativeIceServers.size());
    native.event_queue_capacity_bytes = config.eventQueueCapacityBytes;

    EasyDeskMediaError error{};
    error.size = sizeof(error);
    EasyDeskMediaSession *session = nullptr;
    const EasyDeskMediaStatus status =
        m_state->api.sessionCreate(m_state->runtime, &native, &session, &error);
    if (status != EASYDESK_MEDIA_STATUS_OK || session == nullptr) {
        m_error = QString::fromUtf8(error.message).trimmed();
        if (m_error.isEmpty())
            m_error = QStringLiteral("Media session creation failed with status %1.")
                          .arg(static_cast<int>(status));
        return nullptr;
    }

    m_error.clear();
    return std::unique_ptr<MediaSession>(new MediaSession(m_state, session));
}

QString MediaRuntimeLibrary::errorString() const {
    return m_error;
}

QString MediaRuntimeLibrary::libraryPath() const {
    return m_libraryPath;
}
