#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QTimer>

class MediaSession;

class SessionVideoMonitor final : public QObject {
    Q_OBJECT

  public:
    explicit SessionVideoMonitor(QObject *parent = nullptr);

    bool start(MediaSession &session);
    void stop();
    void transportConnected();
    void transportDisconnected();
    bool firstFramePresented() const;
    QSize presentedFrameSize() const;

  signals:
    void firstFrame(quint32 width, quint32 height);
    void framePresented();
    void failed(const QString &code, const QString &message);

  private:
    void poll();

    MediaSession *m_session = nullptr;
    QTimer m_timer;
    QElapsedTimer m_firstFrameDeadline;
    bool m_connected = false;
    bool m_firstFramePresented = false;
    bool m_hadConnection = false;
    quint64 m_minimumFrameCount = 0;
    quint64 m_lastReportedFrameCount = 0;
    QSize m_presentedFrameSize;
};
