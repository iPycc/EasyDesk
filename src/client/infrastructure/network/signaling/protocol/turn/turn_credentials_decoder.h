#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace easydesk::client::signaling {

struct TurnCredentialsMessage {
    QByteArray requestId;
    QByteArray sessionId;
    quint32 sessionEpoch = 0;
    QStringList urls;
    QString username;
    QString password;
    qint64 expiresAt = 0;
};

bool decodeTurnCredentials(const QByteArray &message,
                           TurnCredentialsMessage &credentials,
                           QString &error);

}  // namespace easydesk::client::signaling
