#pragma once

#include "easydesk_media_abi.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <windows.h>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"

namespace easydesk::media {

class PeerConnectionSession;

class SessionVideoRendererRegistry final {
  public:
    static SessionVideoRendererRegistry &instance();

    EasyDeskMediaStatus attach(PeerConnectionSession *session,
                               HWND window,
                               std::string *error);
    void bindRemoteTrack(
        PeerConnectionSession *session,
        webrtc::scoped_refptr<webrtc::VideoTrackInterface> track);
    void detach(PeerConnectionSession *session);

  private:
    struct Entry;

    SessionVideoRendererRegistry() = default;
    ~SessionVideoRendererRegistry();

    SessionVideoRendererRegistry(const SessionVideoRendererRegistry &) = delete;
    SessionVideoRendererRegistry &operator=(const SessionVideoRendererRegistry &) = delete;

    std::mutex m_mutex;
    std::unordered_map<PeerConnectionSession *, std::unique_ptr<Entry>> m_entries;
};

}  // namespace easydesk::media
