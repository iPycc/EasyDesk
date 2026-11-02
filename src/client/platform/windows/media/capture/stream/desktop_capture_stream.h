#pragma once

#include "media/capture/dxgi_desktop_capture.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

class DesktopCaptureStream final {
  public:
    enum class State {
        Idle,
        Starting,
        Capturing,
        Recovering,
        Failed,
        Stopping,
    };

    struct Statistics {
        quint64 capturedFrames = 0;
        quint64 droppedFrames = 0;
        quint64 displayGeneration = 0;
    };

    DesktopCaptureStream() = default;
    ~DesktopCaptureStream();

    DesktopCaptureStream(const DesktopCaptureStream &) = delete;
    DesktopCaptureStream &operator=(const DesktopCaptureStream &) = delete;

    bool start(int displayIndex);
    void stop();
    bool takeLatestFrame(DesktopCaptureFrame &frame,
                         std::chrono::milliseconds timeout);

    State state() const;
    HRESULT lastError() const;
    DesktopDisplayDescriptor display() const;
    Statistics statistics() const;

  private:
    static constexpr std::size_t kMaximumQueuedFrames = 2;

    void run();
    bool reopenCapture(DxgiDesktopCapture &capture);
    void publishCaptureReady(const DesktopDisplayDescriptor &display);
    void publishFailure(HRESULT error);

    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::thread m_worker;
    std::deque<DesktopCaptureFrame> m_frames;
    DesktopDisplayDescriptor m_display;
    Statistics m_statistics;
    State m_state = State::Idle;
    HRESULT m_lastError = S_OK;
    int m_displayIndex = -1;
    bool m_stopRequested = false;
};
