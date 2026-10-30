#include "media/webrtc/media_runtime_state.h"

#include <windows.h>

MediaRuntimeState::~MediaRuntimeState() {
    if (runtime != nullptr && api.runtimeDestroy != nullptr)
        api.runtimeDestroy(runtime);
    runtime = nullptr;
    if (module != nullptr)
        FreeLibrary(static_cast<HMODULE>(module));
    module = nullptr;
}
