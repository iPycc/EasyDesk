#include "session/data_channel_observers.h"

#include "session/media_event_queue.h"
#include "runtime/media_runtime_internal.h"

#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace easydesk::media {
namespace {

static_assert(static_cast<int>(webrtc::DataChannelInterface::kOpen) == EASYDESK_MEDIA_DATA_CHANNEL_OPEN);
static_assert(static_cast<int>(webrtc::DataChannelInterface::kClosing) == EASYDESK_MEDIA_DATA_CHANNEL_CLOSING);
static_assert(static_cast<int>(webrtc::DataChannelInterface::kClosed) == EASYDESK_MEDIA_DATA_CHANNEL_CLOSED);

constexpr size_t kMaximumControlMessageBytes = 4096;
constexpr size_t kMaximumInputMessageBytes = 16384;
constexpr size_t kMaximumPointerMessageBytes = 256;
constexpr size_t kMaximumFileMessageBytes = 16384;
constexpr uint64_t kMaximumReliableBufferedBytes = 64u * 1024u;
constexpr uint64_t kMaximumFileBufferedBytes = 2u * 1024u * 1024u;

bool channelPermitted(const std::string &label, uint64_t capabilities) {
    if (label == "control.v1")
        return true;
    if (label == "input.v1" || label == "pointer.v1")
        return (capabilities & EASYDESK_MEDIA_CAPABILITY_SEND_INPUT) != 0;
    if (label == "files.meta.v1" || label == "files.data.v1")
        return (capabilities & EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES) != 0;
    return false;
}

size_t maximumMessageBytes(const std::string &label) {
    if (label == "control.v1")
        return kMaximumControlMessageBytes;
    if (label == "input.v1")
        return kMaximumInputMessageBytes;
    if (label == "pointer.v1")
        return kMaximumPointerMessageBytes;
    if (label == "files.meta.v1" || label == "files.data.v1")
        return kMaximumFileMessageBytes;
    return 0;
}

class ChannelObserver final : public webrtc::DataChannelObserver {
  public:
    ChannelObserver(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                    std::shared_ptr<MediaEventQueue> events,
                    const std::atomic<uint64_t> *capabilities,
                    uint32_t iceGeneration)
        : m_channel(std::move(channel)), m_events(std::move(events)),
          m_capabilities(capabilities), m_iceGeneration(iceGeneration),
          m_label(m_channel->label()) {}

    void start() {
        m_channel->RegisterObserver(this);
        publishState();
    }

    ~ChannelObserver() override {
        m_channel->UnregisterObserver();
    }

    void OnStateChange() override { publishState(); }
    void OnMessage(const webrtc::DataBuffer &buffer) override {
        if (!channelPermitted(m_label, m_capabilities->load()))
            return;
        const size_t maximum = maximumMessageBytes(m_label);
        if (!buffer.binary || buffer.size() == 0 || buffer.size() > maximum) {
            const std::string message = "Invalid data channel message.";
            MediaEventRecord event;
            event.type = EASYDESK_MEDIA_EVENT_ERROR;
            event.code = EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
            event.iceGeneration = m_iceGeneration;
            event.payload.assign(message.begin(), message.end());
            m_events->push(std::move(event));
            return;
        }
        MediaEventRecord event;
        event.type = EASYDESK_MEDIA_EVENT_DATA_MESSAGE;
        event.iceGeneration = m_iceGeneration;
        event.labelSize = static_cast<uint32_t>(m_label.size());
        event.payload.reserve(m_label.size() + buffer.size());
        event.payload.insert(event.payload.end(), m_label.begin(), m_label.end());
        const auto *bytes = buffer.data.cdata<uint8_t>();
        event.payload.insert(event.payload.end(), bytes, bytes + buffer.size());
        m_events->push(std::move(event));
    }

    EasyDeskMediaStatus send(const uint8_t *payload, uint32_t size, std::string &error) {
        if (!channelPermitted(m_label, m_capabilities->load())) {
            error = "Data channel permission is not granted.";
            return EASYDESK_MEDIA_STATUS_PERMISSION_DENIED;
        }
        if (m_channel->state() != webrtc::DataChannelInterface::kOpen) {
            error = "Data channel is not open.";
            return EASYDESK_MEDIA_STATUS_INVALID_STATE;
        }
        const uint64_t buffered = m_channel->buffered_amount();
        const uint64_t limit = m_label == "files.data.v1"
                                   ? kMaximumFileBufferedBytes
                                   : kMaximumReliableBufferedBytes;
        if ((m_label == "pointer.v1" && buffered != 0) || buffered > limit ||
            size > limit - buffered) {
            error = "Data channel send queue is full.";
            return EASYDESK_MEDIA_STATUS_WOULD_BLOCK;
        }
        webrtc::CopyOnWriteBuffer bytes(payload, size);
        if (!m_channel->Send(webrtc::DataBuffer(bytes, true))) {
            error = "Data channel could not accept the message.";
            return EASYDESK_MEDIA_STATUS_WOULD_BLOCK;
        }
        error.clear();
        return EASYDESK_MEDIA_STATUS_OK;
    }

  private:
    void publishState() {
        const auto state = m_channel->state();
        if (state == m_lastState)
            return;
        m_lastState = state;
        if (state == webrtc::DataChannelInterface::kConnecting)
            return;
        MediaEventRecord event;
        event.type = EASYDESK_MEDIA_EVENT_DATA_CHANNEL;
        event.code = static_cast<int32_t>(state);
        event.iceGeneration = m_iceGeneration;
        event.payload.assign(m_label.begin(), m_label.end());
        m_events->push(std::move(event));
    }

    webrtc::scoped_refptr<webrtc::DataChannelInterface> m_channel;
    std::shared_ptr<MediaEventQueue> m_events;
    const std::atomic<uint64_t> *m_capabilities;
    uint32_t m_iceGeneration;
    std::string m_label;
    webrtc::DataChannelInterface::DataState m_lastState =
        webrtc::DataChannelInterface::kConnecting;
};

using ChannelMap = std::map<std::string, std::unique_ptr<ChannelObserver>>;

std::map<PeerConnectionSession *, ChannelMap> &observers() {
    static std::map<PeerConnectionSession *, ChannelMap> registry;
    return registry;
}

std::mutex &runtimeMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<PeerConnectionSession *, std::shared_ptr<MediaRuntime>> &runtimes() {
    static std::map<PeerConnectionSession *, std::shared_ptr<MediaRuntime>> registry;
    return registry;
}

}  // namespace

void registerDataChannelSession(PeerConnectionSession *session,
                                std::shared_ptr<MediaRuntime> runtime) {
    std::lock_guard<std::mutex> lock(runtimeMutex());
    runtimes()[session] = std::move(runtime);
}

void unregisterDataChannelSession(PeerConnectionSession *session) {
    std::lock_guard<std::mutex> lock(runtimeMutex());
    runtimes().erase(session);
}

EasyDeskMediaStatus sendDataChannelMessage(PeerConnectionSession *session,
                                           const std::string &label,
                                           const uint8_t *payload,
                                           uint32_t payloadSize,
                                           std::string *error) {
    std::string failure;
    const size_t maximum = maximumMessageBytes(label);
    if (!session || !payload || payloadSize == 0 || maximum == 0 || payloadSize > maximum) {
        if (error)
            *error = "Invalid data channel label or message size.";
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    std::shared_ptr<MediaRuntime> runtime;
    {
        std::lock_guard<std::mutex> lock(runtimeMutex());
        const auto found = runtimes().find(session);
        if (found != runtimes().end())
            runtime = found->second;
    }
    if (!runtime || !runtime->signalingThread()) {
        if (error)
            *error = "Media session is closed.";
        return EASYDESK_MEDIA_STATUS_INVALID_STATE;
    }
    EasyDeskMediaStatus status = EASYDESK_MEDIA_STATUS_INVALID_STATE;
    auto send = [&] {
        std::lock_guard<std::mutex> lock(runtimeMutex());
        const auto runtimeEntry = runtimes().find(session);
        if (runtimeEntry == runtimes().end() || runtimeEntry->second != runtime) {
            failure = "Media session is closed.";
            return;
        }
        const auto found = observers().find(session);
        if (found == observers().end()) {
            failure = "Data channel is unavailable.";
            return;
        }
        const auto channel = found->second.find(label);
        if (channel == found->second.end()) {
            failure = "Data channel is unavailable.";
            return;
        }
        status = channel->second->send(payload, payloadSize, failure);
    };
    if (runtime->signalingThread()->IsCurrent())
        send();
    else
        runtime->signalingThread()->BlockingCall(send);
    if (error)
        *error = std::move(failure);
    return status;
}

bool observeDataChannel(PeerConnectionSession *session,
                        webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                        std::shared_ptr<MediaEventQueue> events,
                        const std::atomic<uint64_t> *capabilities,
                        uint32_t iceGeneration) {
    if (!session || !channel || !events || !capabilities || iceGeneration == 0)
        return false;
    const std::string label = channel->label();
    auto &channels = observers()[session];
    if (channels.find(label) != channels.end())
        return false;
    auto observer = std::make_unique<ChannelObserver>(std::move(channel),
                                                       std::move(events), capabilities,
                                                       iceGeneration);
    observer->start();
    channels.emplace(label, std::move(observer));
    return true;
}

void removeDataChannelObservers(PeerConnectionSession *session) {
    observers().erase(session);
}

}  // namespace easydesk::media
