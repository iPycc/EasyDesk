#include "session/media_event_queue.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace easydesk::media {
namespace {

constexpr std::size_t kDefaultQueueCapacity = 512u * 1024u;
constexpr std::size_t kMinimumQueueCapacity = 64u * 1024u;
constexpr std::size_t kMaximumQueueCapacity = 2u * 1024u * 1024u;

}  // namespace

MediaEventQueue::MediaEventQueue(uint32_t capacityBytes)
    : m_capacityBytes(std::clamp<std::size_t>(
          capacityBytes == 0 ? kDefaultQueueCapacity : capacityBytes,
          kMinimumQueueCapacity, kMaximumQueueCapacity)) {}

bool MediaEventQueue::push(MediaEventRecord event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed || m_overflowed)
        return false;
    if (event.payload.size() > EASYDESK_MEDIA_MAX_EVENT_PAYLOAD_SIZE ||
        m_events.size() >= kMaximumEventCount ||
        event.payload.size() > m_capacityBytes - m_queuedBytes) {
        m_overflowed = true;
        m_closed = true;
        m_condition.notify_all();
        return false;
    }

    m_queuedBytes += event.payload.size();
    m_events.push_back(std::move(event));
    m_condition.notify_one();
    return true;
}

EasyDeskMediaStatus MediaEventQueue::poll(EasyDeskMediaEvent *event,
                                          uint8_t *payload,
                                          uint32_t capacity,
                                          uint32_t *needed,
                                          uint32_t timeoutMs) {
    if (event == nullptr || event->size < sizeof(EasyDeskMediaEvent) || needed == nullptr)
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_events.empty() && !m_closed && !m_overflowed && timeoutMs > 0) {
        m_condition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
            return !m_events.empty() || m_closed || m_overflowed;
        });
    }
    if (m_events.empty()) {
        *needed = 0;
        if (m_overflowed)
            return EASYDESK_MEDIA_STATUS_QUEUE_OVERFLOW;
        return m_closed ? EASYDESK_MEDIA_STATUS_SESSION_CLOSED
                        : EASYDESK_MEDIA_STATUS_NO_EVENT;
    }

    const MediaEventRecord &front = m_events.front();
    *needed = static_cast<uint32_t>(front.payload.size());
    if (capacity < front.payload.size() || (!front.payload.empty() && payload == nullptr))
        return EASYDESK_MEDIA_STATUS_BUFFER_TOO_SMALL;

    event->type = front.type;
    event->sequence = m_nextSequence++;
    event->code = front.code;
    event->ice_generation = front.iceGeneration;
    event->sdp_mline_index = front.sdpMlineIndex;
    event->label_size = front.labelSize;
    event->payload_size = static_cast<uint32_t>(front.payload.size());
    std::fill(std::begin(event->reserved), std::end(event->reserved), 0u);
    if (!front.payload.empty())
        std::memcpy(payload, front.payload.data(), front.payload.size());

    m_queuedBytes -= front.payload.size();
    m_events.pop_front();
    return EASYDESK_MEDIA_STATUS_OK;
}

void MediaEventQueue::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    m_condition.notify_all();
}

}  // namespace easydesk::media
