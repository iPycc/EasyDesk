#include "session/session_description_observers.h"

#include "session/media_event_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"

namespace easydesk::media {
namespace {

std::vector<uint8_t> bytesFromString(const std::string &value) {
    return std::vector<uint8_t>(value.begin(), value.end());
}

void pushOrClose(const std::shared_ptr<MediaEventQueue> &events,
                 const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> &peer,
                 MediaEventRecord event) {
    if (!events->push(std::move(event)) && peer)
        peer->Close();
}

void pushRtcError(const std::shared_ptr<MediaEventQueue> &events,
                  const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> &peer,
                  webrtc::RTCError error,
                  uint32_t iceGeneration) {
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_ERROR;
    event.code = static_cast<int32_t>(error.type());
    event.iceGeneration = iceGeneration;
    event.payload = bytesFromString(error.message());
    pushOrClose(events, peer, std::move(event));
}

class SetLocalObserver : public webrtc::SetLocalDescriptionObserverInterface {
  public:
    SetLocalObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                     std::shared_ptr<MediaEventQueue> events,
                     EasyDeskMediaSdpType type,
                     std::string sdp,
                     uint32_t iceGeneration)
        : m_peer(std::move(peer)),
          m_events(std::move(events)),
          m_type(type),
          m_sdp(std::move(sdp)),
          m_iceGeneration(iceGeneration) {}

    void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
        if (!error.ok()) {
            pushRtcError(m_events, m_peer, std::move(error), m_iceGeneration);
            return;
        }
        MediaEventRecord event;
        event.type = EASYDESK_MEDIA_EVENT_LOCAL_DESCRIPTION;
        event.code = static_cast<int32_t>(m_type);
        event.iceGeneration = m_iceGeneration;
        event.payload = bytesFromString(m_sdp);
        pushOrClose(m_events, m_peer, std::move(event));
    }

  private:
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peer;
    std::shared_ptr<MediaEventQueue> m_events;
    EasyDeskMediaSdpType m_type;
    std::string m_sdp;
    uint32_t m_iceGeneration;
};

class CreateDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
  public:
    CreateDescriptionObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                              std::shared_ptr<MediaEventQueue> events,
                              uint32_t iceGeneration)
        : m_peer(std::move(peer)),
          m_events(std::move(events)),
          m_iceGeneration(iceGeneration) {}

    void OnSuccess(webrtc::SessionDescriptionInterface *description) override {
        std::unique_ptr<webrtc::SessionDescriptionInterface> owned(description);
        if (!owned)
            return;

        std::string sdp;
        if (!owned->ToString(&sdp)) {
            pushRtcError(m_events, m_peer,
                         webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                          "Failed to serialize local SDP."),
                         m_iceGeneration);
            return;
        }
        const EasyDeskMediaSdpType type =
            owned->GetType() == webrtc::SdpType::kOffer ? EASYDESK_MEDIA_SDP_OFFER
                                                        : EASYDESK_MEDIA_SDP_ANSWER;
        m_peer->SetLocalDescription(
            std::move(owned),
            webrtc::make_ref_counted<SetLocalObserver>(m_peer, m_events, type, std::move(sdp),
                                                       m_iceGeneration));
    }

    void OnFailure(webrtc::RTCError error) override {
        pushRtcError(m_events, m_peer, std::move(error), m_iceGeneration);
    }

  private:
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peer;
    std::shared_ptr<MediaEventQueue> m_events;
    uint32_t m_iceGeneration;
};

class SetRemoteObserver : public webrtc::SetRemoteDescriptionObserverInterface {
  public:
    SetRemoteObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                      std::shared_ptr<MediaEventQueue> events,
                      EasyDeskMediaSdpType type,
                      uint32_t iceGeneration)
        : m_peer(std::move(peer)),
          m_events(std::move(events)),
          m_type(type),
          m_iceGeneration(iceGeneration) {}

    void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
        if (!error.ok()) {
            pushRtcError(m_events, m_peer, std::move(error), m_iceGeneration);
            return;
        }
        MediaEventRecord event;
        event.type = EASYDESK_MEDIA_EVENT_REMOTE_DESCRIPTION_APPLIED;
        event.code = static_cast<int32_t>(m_type);
        event.iceGeneration = m_iceGeneration;
        pushOrClose(m_events, m_peer, std::move(event));
    }

  private:
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peer;
    std::shared_ptr<MediaEventQueue> m_events;
    EasyDeskMediaSdpType m_type;
    uint32_t m_iceGeneration;
};

}  // namespace

webrtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver>
createDescriptionObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                          std::shared_ptr<MediaEventQueue> events,
                          uint32_t iceGeneration) {
    return webrtc::make_ref_counted<CreateDescriptionObserver>(std::move(peer),
                                                               std::move(events),
                                                               iceGeneration);
}

webrtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>
createRemoteDescriptionObserver(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer,
                                std::shared_ptr<MediaEventQueue> events,
                                EasyDeskMediaSdpType type,
                                uint32_t iceGeneration) {
    return webrtc::make_ref_counted<SetRemoteObserver>(std::move(peer), std::move(events), type,
                                                       iceGeneration);
}

}  // namespace easydesk::media
