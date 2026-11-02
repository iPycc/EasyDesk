#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "api/scoped_refptr.h"
#include "media/base/adapted_video_track_source.h"
#include "modules/desktop_capture/desktop_capturer.h"

namespace easydesk::media {

class DesktopVideoSource : public webrtc::AdaptedVideoTrackSource,
                           private webrtc::DesktopCapturer::Callback {
  public:
    static webrtc::scoped_refptr<DesktopVideoSource> create(int displayIndex,
                                                            uint32_t maximumFps);

    bool start(std::string *error);
    void stop();

    bool is_screencast() const override;
    std::optional<bool> needs_denoising() const override;
    SourceState state() const override;
    bool remote() const override;

    uint64_t capturedFrames() const;
    uint64_t deliveredFrames() const;

  protected:
    DesktopVideoSource(int displayIndex, uint32_t maximumFps);
    ~DesktopVideoSource() override;

  private:
    void captureLoop();
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override;
    void setFailure(std::string error);

    const int m_displayIndex;
    const uint32_t m_maximumFps;
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
