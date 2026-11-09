#include "session/peer_connection_session.h"

#include "common/media_error.h"
#include "video/render/session_video_renderer_registry.h"

#include <string>

EasyDeskMediaStatus easydesk_media_session_attach_video_renderer(
    EasyDeskMediaSession *session,
    void *nativeWindow,
    EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (session == nullptr || !session->implementation || nativeWindow == nullptr) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media session or native video window.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    std::string failure;
    const EasyDeskMediaStatus status = SessionVideoRendererRegistry::instance().attach(
        session->implementation.get(), static_cast<HWND>(nativeWindow), &failure);
    setMediaError(error, status, failure);
    return status;
}

EasyDeskMediaStatus easydesk_media_session_detach_video_renderer(
    EasyDeskMediaSession *session,
    EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (session == nullptr || !session->implementation) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media session handle.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    SessionVideoRendererRegistry::instance().detach(session->implementation.get());
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus easydesk_media_session_video_renderer_stats(
    EasyDeskMediaSession *session,
    EasyDeskMediaVideoRendererStats *stats,
    EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (session == nullptr || !session->implementation) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media session handle.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    std::string failure;
    const auto status = SessionVideoRendererRegistry::instance().statistics(
        session->implementation.get(), stats, &failure);
    setMediaError(error, status, failure);
    return status;
}
