#pragma once

#include "easydesk_media_abi.h"

#include <memory>

#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

namespace easydesk::media {

class MediaEventQueue;

webrtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver>
createDescriptionObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                          std::shared_ptr<MediaEventQueue> events,
                          uint32_t iceGeneration);

webrtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>
createRemoteDescriptionObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                                std::shared_ptr<MediaEventQueue> events,
                                EasyDeskMediaSdpType type,
                                uint32_t iceGeneration);

}  // namespace easydesk::media
