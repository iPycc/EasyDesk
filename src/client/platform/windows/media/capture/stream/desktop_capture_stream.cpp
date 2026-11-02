#include "media/capture/stream/desktop_capture_stream.h"

#include <utility>

namespace {
constexpr auto kInitialStartTimeout = std::chrono::seconds(5);
constexpr auto kRecoveryDelay = std::chrono::milliseconds(250);
constexpr unsigned int kCaptureWaitMilliseconds = 100;
} // namespace

DesktopCaptureStream::~DesktopCaptureStream() {
    stop();
}

bool DesktopCaptureStream::start(int displayIndex) {
    stop();
    if (displayIndex < 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = E_INVALIDARG;
        m_state = State::Failed;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_displayIndex = displayIndex;
        m_stopRequested = false;
        m_lastError = S_OK;
        m_frames.clear();
        m_display = {};
        m_statistics = {};
        m_state = State::Starting;
    }
    m_worker = std::thread(&DesktopCaptureStream::run, this);

    std::unique_lock<std::mutex> lock(m_mutex);
    const bool started = m_changed.wait_for(lock, kInitialStartTimeout, [this] {
        return m_state == State::Capturing || m_state == State::Failed;
    });
    if (started && m_state == State::Capturing)
        return true;
    if (!started)
        m_lastError = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    lock.unlock();
    stop();
    return false;
}

void DesktopCaptureStream::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_worker.joinable()) {
            if (m_state != State::Failed)
                m_state = State::Idle;
            m_frames.clear();
            return;
        }
        m_stopRequested = true;
        m_state = State::Stopping;
    }
    m_changed.notify_all();
    m_worker.join();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.clear();
    m_state = State::Idle;
    m_changed.notify_all();
}

bool DesktopCaptureStream::takeLatestFrame(
    DesktopCaptureFrame &frame,
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(m_mutex);
    const bool ready = m_changed.wait_for(lock, timeout, [this] {
        return !m_frames.empty() || m_state == State::Failed ||
               m_state == State::Idle || m_state == State::Stopping;
    });
    if (!ready || m_frames.empty())
        return false;

    while (m_frames.size() > 1) {
        m_frames.pop_front();
        ++m_statistics.droppedFrames;
    }
    frame = std::move(m_frames.back());
    m_frames.clear();
    return true;
}

DesktopCaptureStream::State DesktopCaptureStream::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

HRESULT DesktopCaptureStream::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

DesktopDisplayDescriptor DesktopCaptureStream::display() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display;
}

DesktopCaptureStream::Statistics DesktopCaptureStream::statistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

void DesktopCaptureStream::run() {
    DxgiDesktopCapture capture;
    if (!reopenCapture(capture))
        return;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopRequested)
                break;
        }

        DesktopCaptureResult result = capture.capture(kCaptureWaitMilliseconds);
        switch (result.status) {
        case DesktopCaptureStatus::Frame: {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopRequested)
                break;
            while (m_frames.size() >= kMaximumQueuedFrames) {
                m_frames.pop_front();
                ++m_statistics.droppedFrames;
            }
            m_statistics.capturedFrames += result.frame.accumulatedFrames > 0
                                              ? result.frame.accumulatedFrames
                                              : 1;
            m_statistics.displayGeneration = result.frame.displayGeneration;
            m_frames.push_back(std::move(result.frame));
            m_lastError = S_OK;
            m_changed.notify_all();
            break;
        }
        case DesktopCaptureStatus::Timeout:
            break;
        case DesktopCaptureStatus::AccessLost:
        case DesktopCaptureStatus::DeviceLost: {
            capture.stop();
            std::unique_lock<std::mutex> lock(m_mutex);
            m_lastError = result.error;
            m_state = State::Recovering;
            m_frames.clear();
            m_changed.notify_all();
            if (m_changed.wait_for(lock, kRecoveryDelay,
                                   [this] { return m_stopRequested; }))
                break;
            lock.unlock();
            if (!reopenCapture(capture))
                return;
            break;
        }
        case DesktopCaptureStatus::WrongThread:
        case DesktopCaptureStatus::Failed:
            publishFailure(result.error);
            return;
        }
    }
    capture.stop();
}

bool DesktopCaptureStream::reopenCapture(DxgiDesktopCapture &capture) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopRequested)
                return false;
        }
        if (capture.start(m_displayIndex)) {
            publishCaptureReady(capture.display());
            return true;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastError = capture.lastError();
        if (m_state == State::Starting) {
            m_state = State::Failed;
            m_changed.notify_all();
            return false;
        }
        m_state = State::Recovering;
        m_changed.notify_all();
        if (m_changed.wait_for(lock, kRecoveryDelay,
                               [this] { return m_stopRequested; }))
            return false;
    }
}

void DesktopCaptureStream::publishCaptureReady(
    const DesktopDisplayDescriptor &display) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display = display;
    m_statistics.displayGeneration = display.generation;
    m_lastError = S_OK;
    m_state = State::Capturing;
    m_changed.notify_all();
}

void DesktopCaptureStream::publishFailure(HRESULT error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = error;
    m_state = State::Failed;
    m_frames.clear();
    m_changed.notify_all();
}
