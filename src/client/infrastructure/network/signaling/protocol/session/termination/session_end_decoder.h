#pragma once

#include "network/signaling/protocol/session/termination/session_end_codec.h"

namespace easydesk::client::signaling {

struct SessionEndContext {
    QByteArray requestId;
    QByteArray sessionId;
    QByteArray authorizationPayload;
    QString fromDeviceId;
    QString toDeviceId;
    quint32 sessionEpoch = 0;
};

struct DecodedSessionEnd {
    SessionEndReason reason = SessionEndReason::UserStopped;
    quint64 sequence = 0;
    qint64 expiresAt = 0;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionEndEnvelope(const QByteArray &message,
                              const SessionEndContext &context,
                              DecodedSessionEnd &decoded,
                              QString &error);

}  // namespace easydesk::client::signaling
