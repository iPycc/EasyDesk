#include "video/capture/desktop_video_source.h"

#include <chrono>
#include <cstddef>
#include <utility>

#include <objbase.h>
#include <windows.h>

#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "rtc_base/time_utils.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace easydesk::media {
namespace {

constexpr uint32_t kMinimumFps = 1;
constexpr uint32_t kMaximumFps = 60;
constexpr auto kStartupTimeout = std::chrono::seconds(5);

class ScopedPhysicalCoordinates final {
  public:
    ScopedPhysicalCoordinates()
        : m_previous(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {}
    ~ScopedPhysicalCoordinates() {
        if (m_previous)
            SetThreadDpiAwarenessContext(m_previous);
    }
    bool available() const { return m_previous != nullptr; }
  private:
    DPI_AWARENESS_CONTEXT m_previous;
};

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
    auto *monitors = reinterpret_cast<std::vector<CaptureRect> *>(context);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return FALSE;
    const RECT &rect = info.rcMonitor;
    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return FALSE;
    monitors->push_back({rect.left, rect.top, static_cast<uint32_t>(rect.right - rect.left),
                         static_cast<uint32_t>(rect.bottom - rect.top)});
    return TRUE;
}

}  // namespace

webrtc::scoped_refptr<DesktopVideoSource> DesktopVideoSource::create(
    int displayIndex,
    uint32_t maximumFps,
    GeometryCallback geometryCallback) {
    if (displayIndex < 0 || maximumFps < kMinimumFps ||
        maximumFps > kMaximumFps) {
        return nullptr;
    }
    return webrtc::make_ref_counted<DesktopVideoSource>(displayIndex, maximumFps,
                                                       std::move(geometryCallback));
}

DesktopVideoSource::DesktopVideoSource(int displayIndex, uint32_t maximumFps,
                                       GeometryCallback geometryCallback)
    : webrtc::AdaptedVideoTrackSource(2),
      m_displayIndex(displayIndex),
      m_maximumFps(maximumFps),
      m_geometryCallback(std::move(geometryCallback)) {}

DesktopVideoSource::~DesktopVideoSource() {
    stop();
}

bool DesktopVideoSource::start(std::string *error) {
    if (m_captureThread.joinable()) {
        if (error != nullptr)
            *error = "Desktop capture is already running.";
        return false;
    }

    m_stopRequested.store(false);
    m_state.store(kInitializing);
    m_capturedFrames.store(0);
    m_deliveredFrames.store(0);
    m_geometryTracker = {};
    m_reportedGeometryGeneration = 0;
    m_haveFrameGeometry = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_error.clear();
    }
    m_captureThread = std::thread(&DesktopVideoSource::captureLoop, this);

    std::unique_lock<std::mutex> lock(m_mutex);
    const bool ready = m_changed.wait_for(lock, kStartupTimeout, [this] {
        return m_state.load() != kInitializing;
    });
    if (ready && m_state.load() == kLive)
        return true;
    if (!ready)
        m_error = "Desktop capture initialization timed out.";
    if (error != nullptr)
        *error = m_error.empty() ? "Desktop capture initialization failed." : m_error;
    lock.unlock();
    stop();
    return false;
}

void DesktopVideoSource::stop() {
    m_stopRequested.store(true);
    m_changed.notify_all();
    if (m_captureThread.joinable() &&
        m_captureThread.get_id() != std::this_thread::get_id()) {
        m_captureThread.join();
    }
    m_state.store(kEnded);
}

bool DesktopVideoSource::is_screencast() const {
    return true;
}

std::optional<bool> DesktopVideoSource::needs_denoising() const {
    return false;
}

webrtc::MediaSourceInterface::SourceState DesktopVideoSource::state() const {
    return m_state.load();
}

bool DesktopVideoSource::remote() const {
    return false;
}

uint64_t DesktopVideoSource::capturedFrames() const {
    return m_capturedFrames.load();
}

uint64_t DesktopVideoSource::deliveredFrames() const {
    return m_deliveredFrames.load();
}

void DesktopVideoSource::captureLoop() {
    const ScopedPhysicalCoordinates physicalCoordinates;
    m_physicalCoordinates = physicalCoordinates.available();
    const HRESULT comStatus = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool releaseCom = SUCCEEDED(comStatus);
    if (FAILED(comStatus) && comStatus != RPC_E_CHANGED_MODE) {
        setFailure("COM initialization failed for desktop capture.");
        return;
    }

    webrtc::DesktopCaptureOptions options =
        webrtc::DesktopCaptureOptions::CreateDefault();
    options.set_allow_directx_capturer(true);
    options.set_prefer_cursor_embedded(true);
    std::unique_ptr<webrtc::DesktopCapturer> capturer =
        webrtc::DesktopCapturer::CreateScreenCapturer(options);
    if (!capturer) {
        setFailure("No desktop capture backend is available.");
        if (releaseCom)
            CoUninitialize();
        return;
    }

    webrtc::DesktopCapturer::SourceList displays;
    if (!capturer->GetSourceList(&displays) ||
        m_displayIndex >= static_cast<int>(displays.size()) ||
        !capturer->SelectSource(displays[static_cast<std::size_t>(m_displayIndex)].id)) {
        setFailure("The requested desktop display is unavailable.");
        if (releaseCom)
            CoUninitialize();
        return;
    }

    capturer->SetMaxFrameRate(m_maximumFps);
    capturer->Start(this);

    const auto frameInterval =
        std::chrono::microseconds(1000000 / m_maximumFps);
    auto nextCapture = std::chrono::steady_clock::now();
    while (!m_stopRequested.load()) {
        capturer->CaptureFrame();
        nextCapture += frameInterval;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_changed.wait_until(lock, nextCapture,
                             [this] { return m_stopRequested.load(); });
        if (nextCapture < std::chrono::steady_clock::now() - frameInterval)
            nextCapture = std::chrono::steady_clock::now();
    }

    capturer.reset();
    m_state.store(kEnded);
    m_changed.notify_all();
    if (releaseCom)
        CoUninitialize();
}

void DesktopVideoSource::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
    if (result == webrtc::DesktopCapturer::Result::ERROR_TEMPORARY) {
        if (m_geometryCallback)
            reportGeometry(m_geometryTracker.invalidate());
        OnFrameDropped();
        return;
    }
    if (result != webrtc::DesktopCapturer::Result::SUCCESS || !frame) {
        if (m_geometryCallback)
            reportGeometry(m_geometryTracker.invalidate());
        setFailure("Desktop capture stopped after a permanent error.");
        m_stopRequested.store(true);
        return;
    }

    m_capturedFrames.fetch_add(1);
    const int width = frame->size().width();
    const int height = frame->size().height();
    if (width <= 0 || height <= 0) {
        if (m_geometryCallback)
            reportGeometry(m_geometryTracker.invalidate());
        if (m_state.load() == kInitializing) {
            setFailure("Desktop capture returned an empty frame.");
            m_stopRequested.store(true);
            return;
        }
        OnFrameDropped();
        return;
    }

    int virtualX = 0, virtualY = 0;
    if (m_geometryCallback) {
        virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        if (!m_haveFrameGeometry || m_frameX != frame->top_left().x() ||
            m_frameY != frame->top_left().y() || m_frameWidth != static_cast<uint32_t>(width) ||
            m_frameHeight != static_cast<uint32_t>(height) || m_virtualX != virtualX ||
            m_virtualY != virtualY) {
            reportGeometry(m_geometryTracker.invalidate());
            m_haveFrameGeometry = true;
            m_frameX = frame->top_left().x();
            m_frameY = frame->top_left().y();
            m_frameWidth = static_cast<uint32_t>(width);
            m_frameHeight = static_cast<uint32_t>(height);
            m_virtualX = virtualX;
            m_virtualY = virtualY;
        }
    }

    const bool firstFrame = m_state.load() == kInitializing;
    const int64_t timestampUs = webrtc::TimeMicros();
    int adaptedWidth = width;
    int adaptedHeight = height;
    int cropWidth = width;
    int cropHeight = height;
    int cropX = 0;
    int cropY = 0;
    const bool frameWanted =
        AdaptFrame(width, height, timestampUs, &adaptedWidth, &adaptedHeight,
                   &cropWidth, &cropHeight, &cropX, &cropY);
    if (!frameWanted && !firstFrame) {
        OnFrameDropped();
        return;
    }

    auto source = webrtc::I420Buffer::Create(width, height);
    if (libyuv::ARGBToI420(
            frame->data(), frame->stride(), source->MutableDataY(),
            source->StrideY(), source->MutableDataU(), source->StrideU(),
            source->MutableDataV(), source->StrideV(), width, height) != 0) {
        if (firstFrame) {
            setFailure("Desktop frame conversion to I420 failed.");
            m_stopRequested.store(true);
            return;
        }
        OnFrameDropped();
        return;
    }

    if (firstFrame) {
        m_state.store(kLive);
        m_changed.notify_all();
    }
    if (!frameWanted)
        return;

    if (m_geometryCallback) {
        std::vector<CaptureRect> monitors;
        const bool enumerated = EnumDisplayMonitors(nullptr, nullptr, collectMonitor,
                                                     reinterpret_cast<LPARAM>(&monitors)) != FALSE;
        reportGeometry(m_geometryTracker.update(
            frame->top_left().x(), frame->top_left().y(), static_cast<uint32_t>(width),
            static_cast<uint32_t>(height), virtualX, virtualY, static_cast<uint32_t>(cropX),
            static_cast<uint32_t>(cropY), static_cast<uint32_t>(cropWidth),
            static_cast<uint32_t>(cropHeight), monitors, m_physicalCoordinates && enumerated));
    }

    webrtc::scoped_refptr<webrtc::VideoFrameBuffer> output = source;
    if (adaptedWidth != width || adaptedHeight != height || cropWidth != width ||
        cropHeight != height || cropX != 0 || cropY != 0) {
        auto adapted = webrtc::I420Buffer::Create(adaptedWidth, adaptedHeight);
        adapted->CropAndScaleFrom(*source, cropX, cropY, cropWidth, cropHeight);
        output = adapted;
    }

    OnFrame(webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(output)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(timestampUs)
                .build());
    m_deliveredFrames.fetch_add(1);
}

void DesktopVideoSource::setFailure(std::string error) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_error = std::move(error);
    }
    m_state.store(kEnded);
    m_changed.notify_all();
}

void DesktopVideoSource::reportGeometry(const CaptureGeometry &geometry) {
    if (geometry.generation == m_reportedGeometryGeneration)
        return;
    m_reportedGeometryGeneration = geometry.generation;
    m_geometryCallback(geometry);
}

}  // namespace easydesk::media
