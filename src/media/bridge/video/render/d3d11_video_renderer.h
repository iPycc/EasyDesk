#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <windows.h>

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

namespace easydesk::media {

class D3d11VideoRenderer final
    : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
  public:
    struct Statistics {
        uint64_t receivedFrames = 0;
        uint64_t renderedFrames = 0;
        uint64_t droppedFrames = 0;
        int sourceWidth = 0;
        int sourceHeight = 0;
    };

    explicit D3d11VideoRenderer(HWND window);
    ~D3d11VideoRenderer() override;

    D3d11VideoRenderer(const D3d11VideoRenderer &) = delete;
    D3d11VideoRenderer &operator=(const D3d11VideoRenderer &) = delete;

    bool start(std::string *error);
    void stop();
    bool isReady() const;
    std::string errorString() const;
    Statistics statistics() const;

    void OnFrame(const webrtc::VideoFrame &frame) override;

  private:
    struct RenderResources;

    void renderLoop();
    bool initialize(std::string &error);
    bool renderFrame(const webrtc::VideoFrame &frame, std::string &error, bool &presented);
    void fail(std::string error);

    const HWND m_window;
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::thread m_renderThread;
    std::optional<webrtc::VideoFrame> m_pendingFrame;
    std::unique_ptr<RenderResources> m_resources;
    Statistics m_statistics;
    std::string m_error;
    bool m_started = false;
    bool m_ready = false;
    bool m_failed = false;
    bool m_stopRequested = false;
};

}  // namespace easydesk::media
