#pragma once

#include "sessions/negotiation/session_negotiation_context.h"

#include <QByteArray>
#include <QString>

class SessionReadyExchange final {
  public:
    bool start(const SessionNegotiationContext &context, QString &error);
    void stop();
    QByteArray makeReady(QString &error);
    bool receiveReady(const QByteArray &message, QByteArray &acknowledgement, QString &error);
    bool receiveAcknowledgement(const QByteArray &message, QString &error);
    bool isReady() const;

  private:
    SessionNegotiationContext m_context;
    QByteArray m_localChallenge;
    bool m_readySent = false;
    bool m_peerReady = false;
    bool m_acknowledged = false;
};
