#pragma once

#include "features/sessions/input/input_command.h"
#include "features/sessions/model/session_access.h"

#include <QObject>

class MediaSession;

class SessionInputTransport final : public QObject {
    Q_OBJECT

  public:
    enum class SendResult { Sent, WouldBlock, Unavailable, Failed };

    explicit SessionInputTransport(QObject *parent = nullptr);
    bool start(MediaSession &session, SessionCapabilities capabilities);
    void stop();
    void setSessionReady(bool ready);
    void setChannelOpen(const QByteArray &label, bool open);
    void receive(const QByteArray &label, const QByteArray &message);
    bool isOpen(InputStream stream) const;
    SendResult send(InputStream stream, const QByteArray &message);

  signals:
    void availabilityChanged();
    void messageReceived(InputStream stream, const QByteArray &message);

  private:
    MediaSession *m_session = nullptr;
    bool m_permitted = false;
    bool m_ready = false;
    bool m_reliableOpen = false;
    bool m_pointerOpen = false;
};
