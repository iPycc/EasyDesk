#pragma once

#include "easydesk_media_abi.h"
#include "features/sessions/model/session_access.h"
#include "features/sessions/model/session_state.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <memory>

struct MediaRuntimeState;

struct MediaIceServer {
    QString url;
    QString username;
    QString credential;
};

struct MediaSessionConfig {
    QByteArray sessionId;
    quint64 sessionEpoch = 0;
    quint64 permissionEpoch = 0;
    SessionRole role = SessionRole::Controller;
    SessionCapabilities capabilities;
    quint32 iceGeneration = 1;
    QVector<MediaIceServer> iceServers;
    quint32 eventQueueCapacityBytes = 0;
};

struct MediaSessionEvent {
    quint32 type = 0;
    quint64 sequence = 0;
    qint32 code = 0;
    quint32 iceGeneration = 0;
    qint32 sdpMlineIndex = -1;
    QByteArray label;
    QByteArray payload;
};

class MediaSession final {
  public:
    enum class SendResult { Sent, WouldBlock, Failed };
    ~MediaSession();

    MediaSession(const MediaSession &) = delete;
    MediaSession &operator=(const MediaSession &) = delete;

    bool createOffer();
    bool createAnswer();
    bool setRemoteDescription(quint32 iceGeneration, quint32 sdpType, const QByteArray &sdp);
    bool addIceCandidate(quint32 iceGeneration,
                         const QByteArray &sdpMid,
                         qint32 sdpMlineIndex,
                         const QByteArray &candidate);
    bool setPermissions(quint64 permissionEpoch, SessionCapabilities capabilities);
    SendResult sendData(const QByteArray &channelLabel, const QByteArray &payload);
    bool pollEvent(MediaSessionEvent &event, quint32 timeoutMs = 0);
    bool attachVideoRenderer(quintptr nativeWindow);
    bool detachVideoRenderer();
    void close();

    bool isActive() const;
    QString errorString() const;

  private:
    friend class MediaRuntimeLibrary;
    MediaSession(std::shared_ptr<MediaRuntimeState> state, EasyDeskMediaSession *session);

    bool acceptStatus(EasyDeskMediaStatus status, const EasyDeskMediaError &error);

    std::shared_ptr<MediaRuntimeState> m_state;
    EasyDeskMediaSession *m_session = nullptr;
    bool m_videoRendererAttached = false;
    bool m_closed = false;
    QString m_error;
};
