#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "api/scoped_refptr.h"
#include "media/base/adapted_video_track_source.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "video/capture/capture_geometry.h"

namespace easydesk::media {

class DesktopVideoSource : public webrtc::AdaptedVideoTrackSource,
                           private webrtc::DesktopCapturer::Callback {
  public:
    using GeometryCallback = std::function<void(const CaptureGeometry &)>;
    static webrtc::scoped_refptr<DesktopVideoSource> create(int displayIndex,
                                                            uint32_t maximumFps,
                                                            GeometryCallback geometryCallback = {});

    bool start(std::string *error);
    void stop();

    bool is_screencast() const override;
    std::optional<bool> needs_denoising() const override;
    SourceState state() const override;
    bool remote() const override;

    uint64_t capturedFrames() const;
    uint64_t deliveredFrames() const;

  protected:
    DesktopVideoSource(int displayIndex, uint32_t maximumFps, GeometryCallback geometryCallback);
    ~DesktopVideoSource() override;

  private:
    void captureLoop();
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override;
    void setFailure(std::string error);
    void reportGeometry(const CaptureGeometry &geometry);

    const int m_displayIndex;
    const uint32_t m_maximumFps;
    GeometryCallback m_geometryCallback;
    CaptureGeometryTracker m_geometryTracker;
    uint64_t m_reportedGeometryGeneration = 0;
    bool m_physicalCoordinates = false;
    bool m_haveFrameGeometry = false;
    int32_t m_frameX = 0, m_frameY = 0, m_virtualX = 0, m_virtualY = 0;
    uint32_t m_frameWidth = 0, m_frameHeight = 0;
    std::atomic<SourceState> m_state{kInitializing};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<uint64_t> m_capturedFrames{0};
    std::atomic<uint64_t> m_deliveredFrames{0};
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::thread m_captureThread;
    std::string m_error;
};

}  // namespace easydesk::media
