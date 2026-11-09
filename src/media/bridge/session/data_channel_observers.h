#pragma once

#include <cstdint>
#include <memory>

#include "api/data_channel_interface.h"
#include "api/scoped_refptr.h"

namespace easydesk::media {

class MediaEventQueue;
class PeerConnectionSession;

// All methods are called on the peer connection's signaling thread.
bool observeDataChannel(PeerConnectionSession *session,
                        webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                        std::shared_ptr<MediaEventQueue> events,
                        uint32_t iceGeneration);
void removeDataChannelObservers(PeerConnectionSession *session);

}  // namespace easydesk::media
