#pragma once

#include "network/signaling/protocol/session/session_request_codec.h"

namespace easydesk::client::signaling {

enum class SessionRejectReason : quint8 {
    Declined = 1,
    Unavailable = 2,
};

enum class SessionCancelReason : quint8 {
    UserCancelled = 1,
    NegotiationFailed = 2,
};

struct SessionDecisionFields {
    SessionRequestFields request;
    QByteArray signedRequestPayload;
    quint64 sequence = 1;
    qint64 expiresAt = 0;
};

QByteArray sessionRejectSigningBytes(const SessionDecisionFields &fields,
                                     SessionRejectReason reason,
                                     QString &error);
QByteArray encodeSessionRejectEnvelope(const SessionDecisionFields &fields,
                                       SessionRejectReason reason,
                                       const QByteArray &signature,
                                       QString &error);
QByteArray sessionCancelSigningBytes(const SessionDecisionFields &fields,
                                     SessionCancelReason reason,
                                     QString &error);
QByteArray encodeSessionCancelEnvelope(const SessionDecisionFields &fields,
                                       SessionCancelReason reason,
                                       const QByteArray &signature,
                                       QString &error);

}  // namespace easydesk::client::signaling
