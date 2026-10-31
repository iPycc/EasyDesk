#pragma once

#include "network/signaling/protocol/session/session_capability.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace easydesk::client::signaling {

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
