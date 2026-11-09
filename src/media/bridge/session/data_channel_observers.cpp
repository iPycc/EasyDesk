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

class ChannelObserver final : public webrtc::DataChannelObserver {
  public:
    ChannelObserver(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                    std::shared_ptr<MediaEventQueue> events,
                    uint32_t iceGeneration)
        : m_channel(std::move(channel)), m_events(std::move(events)),
          m_iceGeneration(iceGeneration), m_label(m_channel->label()) {}

    void start() {
        m_channel->RegisterObserver(this);
        publishState();
    }

    ~ChannelObserver() override {
        m_channel->UnregisterObserver();
    }

    void OnStateChange() override { publishState(); }
    void OnMessage(const webrtc::DataBuffer &) override {
        // Input and file message handling is not enabled until authorization,
        // bounded decoding, and transport lifecycle handling are connected.
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
                        uint32_t iceGeneration) {
    if (!session || !channel || !events || iceGeneration == 0)
        return false;
    const std::string label = channel->label();
    auto &channels = observers()[session];
    if (channels.find(label) != channels.end())
        return false;
    auto observer = std::make_unique<ChannelObserver>(std::move(channel),
                                                       std::move(events), iceGeneration);
    observer->start();
    channels.emplace(label, std::move(observer));
    return true;
}

void removeDataChannelObservers(PeerConnectionSession *session) {
    observers().erase(session);
}

}  // namespace easydesk::media
