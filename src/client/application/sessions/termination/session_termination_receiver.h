#pragma once

#include "network/signaling/protocol/session/termination/session_end_codec.h"
#include "sessions/negotiation/session_negotiation_context.h"

#include <QObject>

class MediaRuntimeLibrary;

class SessionTerminationReceiver final : public QObject {
    Q_OBJECT

  public:
    SessionTerminationReceiver(MediaRuntimeLibrary &media,
                               QObject *parent = nullptr);

    bool begin(const SessionNegotiationContext &context, QString &error);
    bool noteRemoteSequence(quint64 sequence, QString &error);
    void stop();
    bool isActive() const;

    void handleEnd(const QByteArray &message);

  signals:
    void ended(easydesk::client::signaling::SessionEndReason reason,
               quint64 sequence);
    void failed(const QString &code, const QString &message);

  private:
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    SessionNegotiationContext m_context;
    quint64 m_lastRemoteSequence = 0;
    bool m_active = false;
};
