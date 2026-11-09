#pragma once

#include <QByteArray>
#include <QObject>
#include <QRect>
#include <QString>
#include <QTimer>

class MediaSession;

class SessionMediaEventPump final : public QObject {
    Q_OBJECT

  public:
    explicit SessionMediaEventPump(QObject *parent = nullptr);

    bool start(MediaSession &session);
    void stop();
    bool isRunning() const;

  signals:
    void localDescription(quint32 iceGeneration,
                          quint32 sdpType,
                          const QByteArray &sdp);
    void localCandidate(quint32 iceGeneration,
                        const QByteArray &sdpMid,
                        qint32 sdpMlineIndex,
                        const QByteArray &candidate);
    void gatheringComplete(quint32 iceGeneration);
    void connectionStateChanged(qint32 state);
    void iceConnectionStateChanged(quint32 iceGeneration, qint32 state);
    void remoteDescriptionApplied(quint32 iceGeneration, quint32 sdpType);
    void permissionChanged();
    void dataChannelOpened(const QByteArray &label);
    void dataChannelClosed(const QByteArray &label);
    void dataChannelMessage(const QByteArray &label, const QByteArray &payload);
    void captureGeometryChanged(const QRect &region, quint64 generation, bool valid);
    void sessionClosed();
    void failed(const QString &message);

  private:
    void drainEvents();
    bool dispatchEvent();
    void fail(const QString &message);

    QTimer m_timer;
    MediaSession *m_session = nullptr;
    quint64 m_lastSequence = 0;
    quint64 m_captureGeometryGeneration = 0;
};
