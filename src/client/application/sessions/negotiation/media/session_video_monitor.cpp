#include "sessions/negotiation/media/session_video_monitor.h"

#include "media/webrtc/media_session.h"

#include <limits>

namespace {
constexpr int kPollIntervalMilliseconds = 250;
constexpr int kFirstFrameTimeoutMilliseconds = 20000;
}  // namespace

SessionVideoMonitor::SessionVideoMonitor(QObject *parent) : QObject(parent) {
    m_timer.setInterval(kPollIntervalMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, &SessionVideoMonitor::poll);
}

bool SessionVideoMonitor::start(MediaSession &session) {
    if (m_session || !session.isActive())
        return false;
    m_session = &session;
    m_connected = false;
    m_firstFramePresented = false;
    m_hadConnection = false;
    m_minimumFrameCount = 0;
    m_lastReportedFrameCount = 0;
    m_presentedFrameSize = {};
    m_firstFrameDeadline.invalidate();
    m_timer.start();
    return true;
}

void SessionVideoMonitor::stop() {
    m_timer.stop();
    m_session = nullptr;
    m_connected = false;
    m_firstFramePresented = false;
    m_hadConnection = false;
    m_minimumFrameCount = 0;
    m_lastReportedFrameCount = 0;
    m_presentedFrameSize = {};
    m_firstFrameDeadline.invalidate();
}

void SessionVideoMonitor::transportConnected() {
    if (!m_session || m_connected)
        return;
    if (m_hadConnection) {
        MediaVideoRendererStats stats;
        if (!m_session->videoRendererStats(stats)) {
            const QString error = m_session->errorString();
            stop();
            emit failed(QStringLiteral("E_VIDEO_RENDERER"), error);
            return;
        }
        m_minimumFrameCount = stats.renderedFrames;
        m_lastReportedFrameCount = stats.renderedFrames;
    }
    m_hadConnection = true;
    m_connected = true;
    if (!m_firstFramePresented)
        m_firstFrameDeadline.start();
    poll();
}

void SessionVideoMonitor::transportDisconnected() {
    if (m_connected)
        m_firstFramePresented = false;
    m_connected = false;
    m_presentedFrameSize = {};
    m_firstFrameDeadline.invalidate();
}

bool SessionVideoMonitor::firstFramePresented() const {
    return m_firstFramePresented;
}

QSize SessionVideoMonitor::presentedFrameSize() const {
    return m_presentedFrameSize;
}

void SessionVideoMonitor::poll() {
    if (!m_session || !m_connected)
        return;
    MediaVideoRendererStats stats;
    if (!m_session->videoRendererStats(stats)) {
        const QString error = m_session->errorString();
        stop();
        emit failed(QStringLiteral("E_VIDEO_RENDERER"), error);
        return;
    }
    if (stats.renderedFrames > m_lastReportedFrameCount) {
        if (stats.sourceWidth == 0 || stats.sourceHeight == 0 ||
            stats.sourceWidth > static_cast<quint32>(std::numeric_limits<int>::max()) ||
            stats.sourceHeight > static_cast<quint32>(std::numeric_limits<int>::max())) {
            stop();
            emit failed(QStringLiteral("E_VIDEO_RENDERER"),
                        QStringLiteral("The presented frame dimensions are invalid."));
            return;
        }
        m_lastReportedFrameCount = stats.renderedFrames;
        m_presentedFrameSize = QSize(static_cast<int>(stats.sourceWidth),
                                     static_cast<int>(stats.sourceHeight));
        emit framePresented();
        if (!m_session || !m_connected)
            return;
    }
    if (!m_firstFramePresented && stats.renderedFrames > m_minimumFrameCount &&
        stats.sourceWidth > 0 && stats.sourceHeight > 0) {
        m_firstFramePresented = true;
        m_firstFrameDeadline.invalidate();
        emit firstFrame(stats.sourceWidth, stats.sourceHeight);
        return;
    }
    if (!m_firstFramePresented && m_firstFrameDeadline.isValid() &&
        m_firstFrameDeadline.elapsed() >= kFirstFrameTimeoutMilliseconds) {
        stop();
        emit failed(QStringLiteral("E_VIDEO_FIRST_FRAME_TIMEOUT"),
                    QStringLiteral("The remote desktop did not produce a first frame."));
    }
}
