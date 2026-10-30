#pragma once

#include "media/webrtc/media_api.h"

struct MediaRuntimeState final {
    ~MediaRuntimeState();

    void *module = nullptr;
    EasyDeskMediaRuntime *runtime = nullptr;
    MediaApi api;
};
