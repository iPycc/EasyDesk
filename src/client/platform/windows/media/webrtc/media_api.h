#pragma once

#include "easydesk_media_abi.h"

struct MediaApi {
    using AbiVersionFunction = uint32_t (*)();
    using IdentityKeyPairFunction = EasyDeskMediaStatus (*)(
        const uint8_t *, uint8_t *, uint8_t *, EasyDeskMediaError *);
    using IdentitySignFunction = EasyDeskMediaStatus (*)(
        const uint8_t *, const uint8_t *, uint32_t, uint8_t *, EasyDeskMediaError *);
    using IdentityVerifyFunction = EasyDeskMediaStatus (*)(
        const uint8_t *, const uint8_t *, uint32_t, const uint8_t *, EasyDeskMediaError *);
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
    using SessionSendDataFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, const char *, uint32_t, const uint8_t *, uint32_t,
        EasyDeskMediaError *);
    using SessionPollEventFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, EasyDeskMediaEvent *, uint8_t *, uint32_t, uint32_t *,
        uint32_t, EasyDeskMediaError *);
    using SessionAttachVideoRendererFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, void *, EasyDeskMediaError *);
    using SessionVideoRendererStatsFunction = EasyDeskMediaStatus (*)(
        EasyDeskMediaSession *, EasyDeskMediaVideoRendererStats *, EasyDeskMediaError *);
    using SessionDestroyFunction = void (*)(EasyDeskMediaSession *);

    AbiVersionFunction abiVersion = nullptr;
    IdentityKeyPairFunction identityKeyPair = nullptr;
    IdentitySignFunction identitySign = nullptr;
    IdentityVerifyFunction identityVerify = nullptr;
    RuntimeCreateFunction runtimeCreate = nullptr;
    RuntimeIsReadyFunction runtimeIsReady = nullptr;
    RuntimeDestroyFunction runtimeDestroy = nullptr;
    SessionCreateFunction sessionCreate = nullptr;
    SessionActionFunction sessionCreateOffer = nullptr;
    SessionActionFunction sessionCreateAnswer = nullptr;
    SessionSetRemoteDescriptionFunction sessionSetRemoteDescription = nullptr;
    SessionAddIceCandidateFunction sessionAddIceCandidate = nullptr;
    SessionSetPermissionsFunction sessionSetPermissions = nullptr;
    SessionSendDataFunction sessionSendData = nullptr;
    SessionPollEventFunction sessionPollEvent = nullptr;
    SessionAttachVideoRendererFunction sessionAttachVideoRenderer = nullptr;
    SessionActionFunction sessionDetachVideoRenderer = nullptr;
    SessionVideoRendererStatsFunction sessionVideoRendererStats = nullptr;
    SessionActionFunction sessionClose = nullptr;
    SessionDestroyFunction sessionDestroy = nullptr;

    bool complete() const {
        return abiVersion && identityKeyPair && identitySign && identityVerify &&
               runtimeCreate && runtimeIsReady && runtimeDestroy &&
               sessionCreate && sessionCreateOffer && sessionCreateAnswer &&
               sessionSetRemoteDescription && sessionAddIceCandidate &&
               sessionSetPermissions && sessionSendData && sessionPollEvent && sessionAttachVideoRenderer &&
               sessionDetachVideoRenderer && sessionVideoRendererStats && sessionClose && sessionDestroy;
    }
};
