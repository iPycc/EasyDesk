#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

struct SessionDeliveryMessage {
    QString messageType;
    QByteArray requestId;
    QByteArray sessionId;
    quint64 sequence = 0;
    bool hasSessionEpoch = false;
    quint32 sessionEpoch = 0;
};

bool decodeSessionDelivery(const QByteArray &message,
                           SessionDeliveryMessage &delivery,
                           QString &error);

}  // namespace easydesk::client::signaling
