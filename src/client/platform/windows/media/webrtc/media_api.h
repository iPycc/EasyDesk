#pragma once

#include "easydesk_media_abi.h"

struct MediaApi {
    using AbiVersionFunction = uint32_t (*)();
    using RuntimeCreateFunction = EasyDeskMediaStatus (*)(
        const EasyDeskMediaRuntimeConfig *, EasyDeskMediaRuntime **, EasyDeskMediaError *);
    using RuntimeIsReadyFunction = int32_t (*)(const EasyDeskMediaRuntime *);
    using RuntimeDestroyFunction = void (*)(EasyDeskMediaRuntime *);
    using SessionCreateFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaRuntime *, const EasyDeskMediaSessionConfig *, EasyDeskMediaSession **,
        EasyDeskMediaError *);
    using SessionActionFunction = EasyDeskMediaStatus (*)(EasyDeskMediaSession *,
                                                          EasyDeskMediaError *);
    using SessionSetRemoteDescriptionFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, uint32_t, uint32_t, const uint8_t *, uint32_t,
        EasyDeskMediaError *);
    using SessionAddIceCandidateFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, uint32_t, const uint8_t *, uint32_t, int32_t,
        const uint8_t *, uint32_t, EasyDeskMediaError *);
    using SessionSetPermissionsFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, uint64_t, uint64_t, EasyDeskMediaError *);
    using SessionPollEventFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, EasyDeskMediaEvent *, uint8_t *, uint32_t, uint32_t *,
        uint32_t, EasyDeskMediaError *);
    using SessionDestroyFunction = void (*)(EasyDeskMediaSession *);

    AbiVersionFunction abiVersion = nullptr;
    RuntimeCreateFunction runtimeCreate = nullptr;
    RuntimeIsReadyFunction runtimeIsReady = nullptr;
    RuntimeDestroyFunction runtimeDestroy = nullptr;
    SessionCreateFunction sessionCreate = nullptr;
    SessionActionFunction sessionCreateOffer = nullptr;
    SessionActionFunction sessionCreateAnswer = nullptr;
    SessionSetRemoteDescriptionFunction sessionSetRemoteDescription = nullptr;
    SessionAddIceCandidateFunction sessionAddIceCandidate = nullptr;
    SessionSetPermissionsFunction sessionSetPermissions = nullptr;
    SessionPollEventFunction sessionPollEvent = nullptr;
    SessionActionFunction sessionClose = nullptr;
    SessionDestroyFunction sessionDestroy = nullptr;

    bool complete() const {
        return abiVersion && runtimeCreate && runtimeIsReady && runtimeDestroy &&
               sessionCreate && sessionCreateOffer && sessionCreateAnswer &&
               sessionSetRemoteDescription && sessionAddIceCandidate &&
               sessionSetPermissions && sessionPollEvent && sessionClose && sessionDestroy;
    }
};
