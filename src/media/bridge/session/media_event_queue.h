#pragma once

#include "easydesk_media_abi.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace easydesk::media {

struct MediaEventRecord {
    uint32_t type = 0;
    int32_t code = 0;
    uint32_t iceGeneration = 0;
    int32_t sdpMlineIndex = -1;
    uint32_t labelSize = 0;
    std::vector<uint8_t> payload;
};

class MediaEventQueue final {
  public:
    explicit MediaEventQueue(uint32_t capacityBytes);

    bool push(MediaEventRecord event);
    EasyDeskMediaStatus poll(EasyDeskMediaEvent *event,
                             uint8_t *payload,
                             uint32_t capacity,
                             uint32_t *needed,
                             uint32_t timeoutMs);
    void close();

  private:
    static constexpr std::size_t kMaximumEventCount = 64;

    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<MediaEventRecord> m_events;
    std::size_t m_capacityBytes;
    std::size_t m_queuedBytes = 0;
    uint64_t m_nextSequence = 1;
    bool m_closed = false;
    bool m_overflowed = false;
};

}  // namespace easydesk::media
