#ifndef EASYDESK_MEDIA_ABI_H
#define EASYDESK_MEDIA_ABI_H

#include <stdint.h>

#if defined(_WIN32) && defined(EASYDESK_MEDIA_BUILD)
#define EASYDESK_MEDIA_API __declspec(dllexport)
#elif defined(_WIN32)
#define EASYDESK_MEDIA_API __declspec(dllimport)
#else
#define EASYDESK_MEDIA_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EASYDESK_MEDIA_ABI_VERSION 5u
#define EASYDESK_MEDIA_ERROR_MESSAGE_CAPACITY 256u
#define EASYDESK_MEDIA_SESSION_ID_SIZE 16u
#define EASYDESK_MEDIA_MAX_EVENT_PAYLOAD_SIZE (256u * 1024u)
#define EASYDESK_IDENTITY_ED25519_SEED_SIZE 32u
#define EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE 32u
#define EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE 64u
#define EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE 64u

typedef struct EasyDeskMediaRuntime EasyDeskMediaRuntime;
typedef struct EasyDeskMediaSession EasyDeskMediaSession;

typedef enum EasyDeskMediaStatus {
    EASYDESK_MEDIA_STATUS_OK = 0,
    EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT = 1,
    EASYDESK_MEDIA_STATUS_ABI_MISMATCH = 2,
    EASYDESK_MEDIA_STATUS_ALREADY_RUNNING = 3,
    EASYDESK_MEDIA_STATUS_SSL_INITIALIZATION_FAILED = 4,
    EASYDESK_MEDIA_STATUS_THREAD_INITIALIZATION_FAILED = 5,
    EASYDESK_MEDIA_STATUS_FACTORY_INITIALIZATION_FAILED = 6,
    EASYDESK_MEDIA_STATUS_INTERNAL_ERROR = 7,
    EASYDESK_MEDIA_STATUS_INVALID_STATE = 8,
    EASYDESK_MEDIA_STATUS_BUFFER_TOO_SMALL = 9,
    EASYDESK_MEDIA_STATUS_NO_EVENT = 10,
    EASYDESK_MEDIA_STATUS_SESSION_CLOSED = 11,
    EASYDESK_MEDIA_STATUS_QUEUE_OVERFLOW = 12,
    EASYDESK_MEDIA_STATUS_SDP_ERROR = 13,
    EASYDESK_MEDIA_STATUS_ICE_ERROR = 14
} EasyDeskMediaStatus;

typedef enum EasyDeskMediaSessionRole {
    EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER = 1,
    EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED = 2
} EasyDeskMediaSessionRole;

typedef enum EasyDeskMediaCapability {
    EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN = 1u << 0,
    EASYDESK_MEDIA_CAPABILITY_SEND_INPUT = 1u << 1,
    EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES = 1u << 2
} EasyDeskMediaCapability;

typedef enum EasyDeskMediaSdpType {
    EASYDESK_MEDIA_SDP_OFFER = 1,
    EASYDESK_MEDIA_SDP_ANSWER = 2
} EasyDeskMediaSdpType;

typedef enum EasyDeskMediaEventType {
    EASYDESK_MEDIA_EVENT_LOCAL_DESCRIPTION = 1,
    EASYDESK_MEDIA_EVENT_LOCAL_CANDIDATE = 2,
    EASYDESK_MEDIA_EVENT_ICE_GATHERING_COMPLETE = 3,
    EASYDESK_MEDIA_EVENT_CONNECTION_STATE = 4,
    EASYDESK_MEDIA_EVENT_ICE_CONNECTION_STATE = 5,
    EASYDESK_MEDIA_EVENT_REMOTE_DESCRIPTION_APPLIED = 6,
    EASYDESK_MEDIA_EVENT_PERMISSION_CHANGED = 7,
    EASYDESK_MEDIA_EVENT_DATA_CHANNEL = 8,
    EASYDESK_MEDIA_EVENT_ERROR = 9
} EasyDeskMediaEventType;

typedef enum EasyDeskMediaConnectionState {
    EASYDESK_MEDIA_CONNECTION_NEW = 0,
    EASYDESK_MEDIA_CONNECTION_CONNECTING = 1,
    EASYDESK_MEDIA_CONNECTION_CONNECTED = 2,
    EASYDESK_MEDIA_CONNECTION_DISCONNECTED = 3,
    EASYDESK_MEDIA_CONNECTION_FAILED = 4,
    EASYDESK_MEDIA_CONNECTION_CLOSED = 5
} EasyDeskMediaConnectionState;

typedef enum EasyDeskMediaIceConnectionState {
    EASYDESK_MEDIA_ICE_NEW = 0,
    EASYDESK_MEDIA_ICE_CHECKING = 1,
    EASYDESK_MEDIA_ICE_CONNECTED = 2,
    EASYDESK_MEDIA_ICE_COMPLETED = 3,
    EASYDESK_MEDIA_ICE_FAILED = 4,
    EASYDESK_MEDIA_ICE_DISCONNECTED = 5,
    EASYDESK_MEDIA_ICE_CLOSED = 6
} EasyDeskMediaIceConnectionState;

typedef enum EasyDeskMediaDataChannelState {
    EASYDESK_MEDIA_DATA_CHANNEL_OPEN = 1,
    EASYDESK_MEDIA_DATA_CHANNEL_CLOSING = 2,
    EASYDESK_MEDIA_DATA_CHANNEL_CLOSED = 3
} EasyDeskMediaDataChannelState;

typedef struct EasyDeskMediaIceServerConfig {
    uint32_t size;
    const char *url_utf8;
    uint32_t url_size;
    const char *username_utf8;
    uint32_t username_size;
    const char *credential_utf8;
    uint32_t credential_size;
} EasyDeskMediaIceServerConfig;

typedef struct EasyDeskMediaSessionConfig {
    uint32_t size;
    uint32_t abi_version;
    uint8_t session_id[EASYDESK_MEDIA_SESSION_ID_SIZE];
    uint64_t session_epoch;
    uint64_t permission_epoch;
    uint64_t capabilities;
    uint32_t role;
    uint32_t ice_generation;
    const EasyDeskMediaIceServerConfig *ice_servers;
    uint32_t ice_server_count;
    uint32_t event_queue_capacity_bytes;
    uint32_t reserved[6];
} EasyDeskMediaSessionConfig;

typedef struct EasyDeskMediaEvent {
    uint32_t size;
    uint32_t type;
    uint64_t sequence;
    int32_t code;
    uint32_t ice_generation;
    int32_t sdp_mline_index;
    uint32_t label_size;
    uint32_t payload_size;
    uint32_t reserved[4];
} EasyDeskMediaEvent;

typedef struct EasyDeskMediaRuntimeConfig {
    uint32_t size;
    uint32_t abi_version;
    uint32_t reserved[6];
} EasyDeskMediaRuntimeConfig;

typedef struct EasyDeskMediaError {
    uint32_t size;
    int32_t code;
    char message[EASYDESK_MEDIA_ERROR_MESSAGE_CAPACITY];
} EasyDeskMediaError;

EASYDESK_MEDIA_API uint32_t easydesk_media_abi_version(void);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_identity_ed25519_keypair_from_seed(
    const uint8_t seed[EASYDESK_IDENTITY_ED25519_SEED_SIZE],
    uint8_t public_key[EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE],
    uint8_t private_key[EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_identity_ed25519_sign(
    const uint8_t private_key[EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
    const uint8_t *message,
    uint32_t message_size,
    uint8_t signature[EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE],
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_identity_ed25519_verify(
    const uint8_t public_key[EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE],
    const uint8_t *message,
    uint32_t message_size,
    const uint8_t signature[EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE],
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_runtime_create(
    const EasyDeskMediaRuntimeConfig *config,
    EasyDeskMediaRuntime **runtime,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API int32_t easydesk_media_runtime_is_ready(
    const EasyDeskMediaRuntime *runtime);

EASYDESK_MEDIA_API void easydesk_media_runtime_destroy(
    EasyDeskMediaRuntime *runtime);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_create(
    EasyDeskMediaRuntime *runtime,
    const EasyDeskMediaSessionConfig *config,
    EasyDeskMediaSession **session,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_create_offer(
    EasyDeskMediaSession *session,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_create_answer(
    EasyDeskMediaSession *session,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_set_remote_description(
    EasyDeskMediaSession *session,
    uint32_t ice_generation,
    uint32_t sdp_type,
    const uint8_t *sdp_utf8,
    uint32_t sdp_size,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_add_ice_candidate(
    EasyDeskMediaSession *session,
    uint32_t ice_generation,
    const uint8_t *sdp_mid_utf8,
    uint32_t sdp_mid_size,
    int32_t sdp_mline_index,
    const uint8_t *candidate_utf8,
    uint32_t candidate_size,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_set_permissions(
    EasyDeskMediaSession *session,
    uint64_t permission_epoch,
    uint64_t capabilities,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_poll_event(
    EasyDeskMediaSession *session,
    EasyDeskMediaEvent *event,
    uint8_t *payload,
    uint32_t payload_capacity,
    uint32_t *payload_needed,
    uint32_t timeout_ms,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_attach_video_renderer(
    EasyDeskMediaSession *session,
    void *native_window,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_detach_video_renderer(
    EasyDeskMediaSession *session,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_session_close(
    EasyDeskMediaSession *session,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API void easydesk_media_session_destroy(
    EasyDeskMediaSession *session);

#ifdef __cplusplus
}
#endif

#endif
