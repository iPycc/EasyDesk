#include "session/data_channel_observers.h"

#include "session/media_event_queue.h"

#include <map>
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

}  // namespace

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
