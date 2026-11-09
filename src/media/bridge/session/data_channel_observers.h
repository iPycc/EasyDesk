#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <string>

#include "easydesk_media_abi.h"

#include "api/data_channel_interface.h"
#include "api/scoped_refptr.h"

namespace easydesk::media {

class MediaEventQueue;
class MediaRuntime;
class PeerConnectionSession;

void registerDataChannelSession(PeerConnectionSession *session,
                                std::shared_ptr<MediaRuntime> runtime);
void unregisterDataChannelSession(PeerConnectionSession *session);
EasyDeskMediaStatus sendDataChannelMessage(PeerConnectionSession *session,
                                           const std::string &label,
                                           const uint8_t *payload,
                                           uint32_t payloadSize,
                                           std::string *error);

// All methods are called on the peer connection's signaling thread.
bool observeDataChannel(PeerConnectionSession *session,
                        webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                        std::shared_ptr<MediaEventQueue> events,
                        const std::atomic<uint64_t> *capabilities,
                        uint32_t iceGeneration);
void removeDataChannelObservers(PeerConnectionSession *session);

}  // namespace easydesk::media
