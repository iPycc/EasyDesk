#pragma once

#include "features/sessions/model/session_access.h"

#include <QObject>

class MediaSession;

enum class FileChannel { Metadata, Data };

class SessionFileTransport final : public QObject {
    Q_OBJECT

  public:
    enum class SendResult { Sent, WouldBlock, Unavailable, Failed };

    explicit SessionFileTransport(QObject *parent = nullptr);
    bool start(MediaSession &session, SessionCapabilities capabilities,
               bool localMayReceive, bool peerMayReceive);
    void stop();
    void setSessionReady(bool ready);
    void setChannelOpen(const QByteArray &label, bool open);
    bool setReceivingEnabled(bool enabled);
    bool setSendingEnabled(bool enabled);
    void receive(const QByteArray &label, const QByteArray &message);
    bool isOpen(FileChannel channel) const;
    SendResult send(FileChannel channel, const QByteArray &message);

  signals:
    void availabilityChanged();
    void messageReceived(FileChannel channel, const QByteArray &message);

  private:
    MediaSession *m_session = nullptr;
    bool m_permitted = false;
    bool m_ready = false;
    bool m_metadataOpen = false;
    bool m_dataOpen = false;
    bool m_localMayReceive = false;
    bool m_peerMayReceive = false;
    bool m_receivingEnabled = false;
    bool m_sendingEnabled = false;
};
