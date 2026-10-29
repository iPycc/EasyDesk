#pragma once

#include "easydesk_media_abi.h"

#include <memory>

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

namespace easydesk::media {

class MediaRuntime final {
  public:
    MediaRuntime() = default;
    ~MediaRuntime();

    bool start(EasyDeskMediaError *error);
    void stop();
    bool ready() const;

    webrtc::PeerConnectionFactoryInterface *factory() const;
    webrtc::Thread *signalingThread() const;

  private:
    std::unique_ptr<webrtc::Thread> m_networkThread;
    std::unique_ptr<webrtc::Thread> m_workerThread;
    std::unique_ptr<webrtc::Thread> m_signalingThread;
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory;
    bool m_sslInitialized = false;
};

}  // namespace easydesk::media

struct EasyDeskMediaRuntime {
    std::shared_ptr<easydesk::media::MediaRuntime> implementation;
};
