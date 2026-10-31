#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace easydesk::client::signaling {

enum class SessionParticipantRole : quint8 {
    Controller = 1,
    Controlled = 2,
};

enum class SessionCapability : quint8 {
    ViewScreen = 1,
    SendInput = 2,
    FileReceive = 3,
};

struct SessionCapabilityGrant {
    QByteArray subjectPublicKey;
    SessionCapability capability = SessionCapability::ViewScreen;
};

struct SessionRequestFields {
    QByteArray requestId;
    QByteArray sessionId;
    QByteArray requesterPublicKey;
    QByteArray targetPublicKey;
    SessionParticipantRole requesterRole = SessionParticipantRole::Controller;
    QByteArray nonce;
    QVector<SessionCapabilityGrant> requestedGrants;
    qint64 expiresAt = 0;
    QString fromDeviceId;
    QString toDeviceId;
};

QByteArray sessionRequestSigningBytes(const SessionRequestFields &fields,
                                      QString &error);
QByteArray encodeSessionRequestEnvelope(const SessionRequestFields &fields,
                                        const QByteArray &signature,
                                        QString &error);

}  // namespace easydesk::client::signaling
