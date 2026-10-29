#include "easydesk_media_abi.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"

namespace {

std::mutex g_runtime_mutex;
bool g_runtime_active = false;
bool g_ssl_initialized = false;

void setError(EasyDeskMediaError *error, EasyDeskMediaStatus code,
              const char *message) {
    if (error == nullptr || error->size < sizeof(EasyDeskMediaError)) {
        return;
    }

    error->code = static_cast<int32_t>(code);
    std::memset(error->message, 0, sizeof(error->message));
    if (message == nullptr) {
        return;
    }

    const std::size_t length = std::min(
        std::strlen(message), sizeof(error->message) - std::size_t{1});
    std::memcpy(error->message, message, length);
}

class MediaRuntime final {
public:
    bool start(EasyDeskMediaError *error) {
        networkThread_ = webrtc::Thread::CreateWithSocketServer();
        workerThread_ = webrtc::Thread::Create();
        signalingThread_ = webrtc::Thread::Create();
        if (!networkThread_ || !workerThread_ || !signalingThread_ ||
            !networkThread_->Start() || !workerThread_->Start() ||
            !signalingThread_->Start()) {
            setError(error, EASYDESK_MEDIA_STATUS_THREAD_INITIALIZATION_FAILED,
                     "WebRTC thread initialization failed.");
            stop();
            return false;
        }

        factory_ = webrtc::CreatePeerConnectionFactory(
            networkThread_.get(), workerThread_.get(), signalingThread_.get(),
            nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            webrtc::CreateBuiltinVideoEncoderFactory(),
            webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);
        if (!factory_) {
            setError(error, EASYDESK_MEDIA_STATUS_FACTORY_INITIALIZATION_FAILED,
                     "WebRTC peer connection factory initialization failed.");
            stop();
            return false;
        }

        return true;
    }

    void stop() {
        factory_ = nullptr;
        if (signalingThread_) {
            signalingThread_->Stop();
        }
        if (workerThread_) {
            workerThread_->Stop();
        }
        if (networkThread_) {
            networkThread_->Stop();
        }
        signalingThread_.reset();
        workerThread_.reset();
        networkThread_.reset();
    }

    bool ready() const {
        return factory_ != nullptr;
    }

private:
    std::unique_ptr<webrtc::Thread> networkThread_;
    std::unique_ptr<webrtc::Thread> workerThread_;
    std::unique_ptr<webrtc::Thread> signalingThread_;
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
};

}  // namespace

struct EasyDeskMediaRuntime {
    MediaRuntime implementation;
};

uint32_t easydesk_media_abi_version(void) {
    return EASYDESK_MEDIA_ABI_VERSION;
}

EasyDeskMediaStatus easydesk_media_runtime_create(
    const EasyDeskMediaRuntimeConfig *config,
    EasyDeskMediaRuntime **runtime,
    EasyDeskMediaError *error) {
    if (runtime != nullptr) {
        *runtime = nullptr;
    }
    if (config == nullptr || runtime == nullptr ||
        config->size < sizeof(EasyDeskMediaRuntimeConfig)) {
        setError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                 "Invalid media runtime arguments.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    if (config->abi_version != EASYDESK_MEDIA_ABI_VERSION) {
        setError(error, EASYDESK_MEDIA_STATUS_ABI_MISMATCH,
                 "Media ABI version mismatch.");
        return EASYDESK_MEDIA_STATUS_ABI_MISMATCH;
    }

    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (g_runtime_active) {
        setError(error, EASYDESK_MEDIA_STATUS_ALREADY_RUNNING,
                 "A media runtime is already active.");
        return EASYDESK_MEDIA_STATUS_ALREADY_RUNNING;
    }
    if (!webrtc::InitializeSSL()) {
        setError(error, EASYDESK_MEDIA_STATUS_SSL_INITIALIZATION_FAILED,
                 "WebRTC SSL initialization failed.");
        return EASYDESK_MEDIA_STATUS_SSL_INITIALIZATION_FAILED;
    }
    g_ssl_initialized = true;

    std::unique_ptr<EasyDeskMediaRuntime> candidate(
        new EasyDeskMediaRuntime{});
    if (!candidate->implementation.start(error)) {
        webrtc::CleanupSSL();
        g_ssl_initialized = false;
        return error != nullptr &&
                       error->size >= sizeof(EasyDeskMediaError)
                   ? static_cast<EasyDeskMediaStatus>(error->code)
                   : EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    }

    g_runtime_active = true;
    *runtime = candidate.release();
    setError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

int32_t easydesk_media_runtime_is_ready(const EasyDeskMediaRuntime *runtime) {
    return runtime != nullptr && runtime->implementation.ready() ? 1 : 0;
}

void easydesk_media_runtime_destroy(EasyDeskMediaRuntime *runtime) {
    if (runtime == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    runtime->implementation.stop();
    delete runtime;
    if (g_ssl_initialized) {
        webrtc::CleanupSSL();
        g_ssl_initialized = false;
    }
    g_runtime_active = false;
}
