#include "session/peer_connection_session.h"

#include "common/media_error.h"
#include "runtime/media_runtime_internal.h"

#include <memory>
#include <utility>

namespace {

bool validSession(EasyDeskMediaSession *session, EasyDeskMediaError *error) {
    if (session != nullptr && session->implementation)
        return true;
    easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                                   "Invalid media session handle.");
    return false;
}

}  // namespace

EasyDeskMediaStatus easydesk_media_session_create(
    EasyDeskMediaRuntime *runtime,
    const EasyDeskMediaSessionConfig *config,
    EasyDeskMediaSession **session,
    EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (session != nullptr)
        *session = nullptr;
    if (runtime == nullptr || !runtime->implementation || config == nullptr || session == nullptr ||
        config->size < sizeof(EasyDeskMediaSessionConfig)) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media session arguments.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    auto implementation = std::make_unique<PeerConnectionSession>(
        runtime->implementation, config->event_queue_capacity_bytes);
    const EasyDeskMediaStatus status = implementation->start(*config, error);
    if (status != EASYDESK_MEDIA_STATUS_OK)
        return status;

    auto handle = std::make_unique<EasyDeskMediaSession>();
    handle->implementation = std::move(implementation);
    *session = handle.release();
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus easydesk_media_session_create_offer(EasyDeskMediaSession *session,
                                                        EasyDeskMediaError *error) {
    return validSession(session, error) ? session->implementation->createOffer(error)
                                        : EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
}

EasyDeskMediaStatus easydesk_media_session_create_answer(EasyDeskMediaSession *session,
                                                         EasyDeskMediaError *error) {
    return validSession(session, error) ? session->implementation->createAnswer(error)
                                        : EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
}

EasyDeskMediaStatus easydesk_media_session_set_remote_description(
    EasyDeskMediaSession *session,
    uint32_t iceGeneration,
    uint32_t sdpType,
    const uint8_t *sdp,
    uint32_t sdpSize,
    EasyDeskMediaError *error) {
    if (!validSession(session, error))
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    return session->implementation->setRemoteDescription(
        iceGeneration, static_cast<EasyDeskMediaSdpType>(sdpType), sdp, sdpSize, error);
}

EasyDeskMediaStatus easydesk_media_session_add_ice_candidate(
    EasyDeskMediaSession *session,
    uint32_t iceGeneration,
    const uint8_t *sdpMid,
    uint32_t sdpMidSize,
    int32_t sdpMlineIndex,
    const uint8_t *candidate,
    uint32_t candidateSize,
    EasyDeskMediaError *error) {
    if (!validSession(session, error))
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    return session->implementation->addIceCandidate(iceGeneration, sdpMid, sdpMidSize,
                                                     sdpMlineIndex, candidate, candidateSize,
                                                     error);
}

EasyDeskMediaStatus easydesk_media_session_set_permissions(EasyDeskMediaSession *session,
                                                           uint64_t permissionEpoch,
                                                           uint64_t capabilities,
                                                           EasyDeskMediaError *error) {
    return validSession(session, error)
               ? session->implementation->setPermissions(permissionEpoch, capabilities, error)
               : EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
}

EasyDeskMediaStatus easydesk_media_session_poll_event(EasyDeskMediaSession *session,
                                                      EasyDeskMediaEvent *event,
                                                      uint8_t *payload,
                                                      uint32_t payloadCapacity,
                                                      uint32_t *payloadNeeded,
                                                      uint32_t timeoutMs,
                                                      EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (!validSession(session, error))
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    const EasyDeskMediaStatus status = session->implementation->pollEvent(
        event, payload, payloadCapacity, payloadNeeded, timeoutMs);
    setMediaError(error, status, "");
    return status;
}

EasyDeskMediaStatus easydesk_media_session_close(EasyDeskMediaSession *session,
                                                 EasyDeskMediaError *error) {
    using namespace easydesk::media;
    if (!validSession(session, error))
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    const EasyDeskMediaStatus status = session->implementation->close();
    setMediaError(error, status, "");
    return status;
}

void easydesk_media_session_destroy(EasyDeskMediaSession *session) {
    delete session;
}
