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

#define EASYDESK_MEDIA_ABI_VERSION 1u
#define EASYDESK_MEDIA_ERROR_MESSAGE_CAPACITY 256u

typedef struct EasyDeskMediaRuntime EasyDeskMediaRuntime;

typedef enum EasyDeskMediaStatus {
    EASYDESK_MEDIA_STATUS_OK = 0,
    EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT = 1,
    EASYDESK_MEDIA_STATUS_ABI_MISMATCH = 2,
    EASYDESK_MEDIA_STATUS_ALREADY_RUNNING = 3,
    EASYDESK_MEDIA_STATUS_SSL_INITIALIZATION_FAILED = 4,
    EASYDESK_MEDIA_STATUS_THREAD_INITIALIZATION_FAILED = 5,
    EASYDESK_MEDIA_STATUS_FACTORY_INITIALIZATION_FAILED = 6,
    EASYDESK_MEDIA_STATUS_INTERNAL_ERROR = 7
} EasyDeskMediaStatus;

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

EASYDESK_MEDIA_API EasyDeskMediaStatus easydesk_media_runtime_create(
    const EasyDeskMediaRuntimeConfig *config,
    EasyDeskMediaRuntime **runtime,
    EasyDeskMediaError *error);

EASYDESK_MEDIA_API int32_t easydesk_media_runtime_is_ready(
    const EasyDeskMediaRuntime *runtime);

EASYDESK_MEDIA_API void easydesk_media_runtime_destroy(
    EasyDeskMediaRuntime *runtime);

#ifdef __cplusplus
}
#endif

#endif
