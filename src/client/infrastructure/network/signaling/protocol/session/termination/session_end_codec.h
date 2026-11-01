#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

enum class SessionEndReason : quint8 {
    UserStopped = 1,
    ConnectionFailed = 2,
    AuthorizationExpired = 3,
    ProtocolFailure = 4,
};

struct SessionEndFields {
    QByteArray requestId;
    QByteArray sessionId;
    QByteArray authorizationPayload;
    QString fromDeviceId;
    QString toDeviceId;
    quint32 sessionEpoch = 0;
    SessionEndReason reason = SessionEndReason::UserStopped;
    quint64 sequence = 0;
    qint64 expiresAt = 0;
};

QByteArray sessionEndSigningBytes(const SessionEndFields &fields,
                                  QString &error);
QByteArray encodeSessionEndEnvelope(const SessionEndFields &fields,
                                    const QByteArray &signature,
                                    QString &error);

}  // namespace easydesk::client::signaling
