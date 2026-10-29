#include "runtime/media_runtime_internal.h"

#include "common/media_error.h"

#include <memory>
#include <mutex>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/ssl_adapter.h"

namespace easydesk::media {
namespace {

std::mutex g_runtimeMutex;
std::weak_ptr<MediaRuntime> g_activeRuntime;

}  // namespace

MediaRuntime::~MediaRuntime() {
    stop();
}

bool MediaRuntime::start(EasyDeskMediaError *error) {
    if (!webrtc::InitializeSSL()) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_SSL_INITIALIZATION_FAILED,
                      "WebRTC SSL initialization failed.");
        return false;
    }
    m_sslInitialized = true;

    m_networkThread = webrtc::Thread::CreateWithSocketServer();
    m_workerThread = webrtc::Thread::Create();
    m_signalingThread = webrtc::Thread::Create();
    if (!m_networkThread || !m_workerThread || !m_signalingThread ||
        !m_networkThread->Start() || !m_workerThread->Start() ||
        !m_signalingThread->Start()) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_THREAD_INITIALIZATION_FAILED,
                      "WebRTC thread initialization failed.");
        stop();
        return false;
    }

    m_factory = webrtc::CreatePeerConnectionFactory(
        m_networkThread.get(), m_workerThread.get(), m_signalingThread.get(), nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);
    if (!m_factory) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_FACTORY_INITIALIZATION_FAILED,
                      "WebRTC peer connection factory initialization failed.");
        stop();
        return false;
    }

    return true;
}

void MediaRuntime::stop() {
    m_factory = nullptr;
    if (m_signalingThread)
        m_signalingThread->Stop();
    if (m_workerThread)
        m_workerThread->Stop();
    if (m_networkThread)
        m_networkThread->Stop();
    m_signalingThread.reset();
    m_workerThread.reset();
    m_networkThread.reset();
    if (m_sslInitialized) {
        webrtc::CleanupSSL();
        m_sslInitialized = false;
    }
}

bool MediaRuntime::ready() const {
    return m_factory != nullptr;
}

webrtc::PeerConnectionFactoryInterface *MediaRuntime::factory() const {
    return m_factory.get();
}

webrtc::Thread *MediaRuntime::signalingThread() const {
    return m_signalingThread.get();
}

}  // namespace easydesk::media

uint32_t easydesk_media_abi_version(void) {
    return EASYDESK_MEDIA_ABI_VERSION;
}

EasyDeskMediaStatus easydesk_media_runtime_create(
    const EasyDeskMediaRuntimeConfig *config,
    EasyDeskMediaRuntime **runtime,
    EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (runtime != nullptr)
        *runtime = nullptr;
    if (config == nullptr || runtime == nullptr ||
        config->size < sizeof(EasyDeskMediaRuntimeConfig)) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media runtime arguments.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    if (config->abi_version != EASYDESK_MEDIA_ABI_VERSION) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_ABI_MISMATCH,
                      "Media ABI version mismatch.");
        return EASYDESK_MEDIA_STATUS_ABI_MISMATCH;
    }

    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    if (!g_activeRuntime.expired()) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_ALREADY_RUNNING,
                      "A media runtime is already active.");
        return EASYDESK_MEDIA_STATUS_ALREADY_RUNNING;
    }
    auto implementation = std::make_shared<MediaRuntime>();
    if (!implementation->start(error))
        return static_cast<EasyDeskMediaStatus>(
            error != nullptr ? error->code : EASYDESK_MEDIA_STATUS_INTERNAL_ERROR);

    auto handle = std::make_unique<EasyDeskMediaRuntime>();
    handle->implementation = implementation;
    g_activeRuntime = implementation;
    *runtime = handle.release();
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

int32_t easydesk_media_runtime_is_ready(const EasyDeskMediaRuntime *runtime) {
    return runtime != nullptr && runtime->implementation && runtime->implementation->ready() ? 1
                                                                                              : 0;
}

void easydesk_media_runtime_destroy(EasyDeskMediaRuntime *runtime) {
    delete runtime;
}
