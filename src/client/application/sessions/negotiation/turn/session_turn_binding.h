#pragma once

#include "sessions/negotiation/session_negotiation_context.h"

#include <QVector>

struct SessionTurnServer {
    QString url;
    QString username;
    QString credential;
};

struct SessionTurnConfiguration {
    QVector<SessionTurnServer> servers;
    qint64 expiresAt = 0;
};

bool bindSessionTurnCredentials(const QByteArray &message,
                                const SessionNegotiationContext &context,
                                qint64 now,
                                SessionTurnConfiguration &configuration,
                                QString &error);
