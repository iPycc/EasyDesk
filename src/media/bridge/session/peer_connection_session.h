#pragma once

#include "easydesk_media_abi.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

namespace easydesk::media {

class MediaEventQueue;
class MediaRuntime;
struct MediaEventRecord;

class PeerConnectionSession final : public webrtc::PeerConnectionObserver {
  public:
    explicit PeerConnectionSession(std::shared_ptr<MediaRuntime> runtime,
                                   uint32_t eventQueueCapacityBytes);
    ~PeerConnectionSession() override;

    EasyDeskMediaStatus start(const EasyDeskMediaSessionConfig &config,
                              EasyDeskMediaError *error);
    EasyDeskMediaStatus createOffer(EasyDeskMediaError *error);
    EasyDeskMediaStatus createAnswer(EasyDeskMediaError *error);
    EasyDeskMediaStatus setRemoteDescription(uint
    32_t iceGeneration,
                                             EasyDeskMediaSdpType type,
                                             const uint8_t *sdp,
                                             uint32_t size,
                                             EasyDeskMediaError *error);
    EasyDeskMediaStatus addIceCandidate(uint32_t iceGeneration,
                                        const uint8_t *sdpMid,
                                        uint32_t sdpMidSize,
                                        int32_t sdpMlineIndex,
                                        const uint8_t *candidate,
                                        uint32_t candidateSize,
                                        EasyDeskMediaError *error);
    EasyDeskMediaStatus setPermissions(uint64_t permissionEpoch,
                                       uint64_t capabilities,
                                       EasyDeskMediaError *error);
    EasyDeskMediaStatus pollEvent(EasyDeskMediaEvent *event,
                                  uint8_t *payload,
                                  uint32_t capacity,
                                  uint32_t *needed,
                                  uint32_t timeoutMs);
    EasyDeskMediaStatus close();

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) override;
    void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState state) override;
    void OnIceCandidate(const webrtc::IceCandidate *candidate) override;
    void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState state) override;
    void OnConnectionChange(
        webrtc::PeerConnectionInterface::PeerConnectionState state) override;

  private:
    bool emitEvent(MediaEventRecord event);

    std::shared_ptr<MediaRuntime> m_runtime;
    std::shared_ptr<MediaEventQueue> m_events;
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peer;
    std::vector<webrtc::scoped_refptr<webrtc::DataChannelInterface>> m_dataChannels;
    std::atomic<uint64_t> m_permissionEpoch{0};
    std::atomic<uint64_t> m_capabilities{0};
    std::atomic<bool> m_closed{false};
    std::atomic<bool> m_cleanupComplete{false};
    uint32_t m_role = 0;
    uint32_t m_iceGeneration = 0;
};

}  // namespace easydesk::media

struct EasyDeskMediaSession {
    std::unique_ptr<easydesk::media::PeerConnectionSession> implementation;
};
