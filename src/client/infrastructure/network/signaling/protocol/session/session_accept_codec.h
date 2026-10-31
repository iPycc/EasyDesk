#pragma once

#include "network/signaling/protocol/session/session_request_codec.h"

namespace easydesk::client::signaling {

struct SessionAcceptFields {
    SessionRequestFields request;
    QByteArray signedRequestPayload;
    QByteArray nonce;
    QVector<SessionCapabilityGrant> grantedGrants;
    quint32 sessionEpoch = 0;
    quint64 permissionEpoch = 1;
    qint64 expiresAt = 0;
};

QByteArray sessionAcceptSigningBytes(const SessionAcceptFields &fields,
                                     QString &error);
QByteArray encodeSessionAcceptEnvelope(const SessionAcceptFields &fields,
                                       const QByteArray &signature,
                                       QString &error);

}  // namespace easydesk::client::signaling
