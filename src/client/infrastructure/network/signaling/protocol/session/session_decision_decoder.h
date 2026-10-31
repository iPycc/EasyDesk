#pragma once

#include "network/signaling/protocol/session/session_decision_codec.h"

namespace easydesk::client::signaling {

enum class SessionDecisionKind {
    Reject,
    Cancel,
};

struct DecodedSessionDecision {
    SessionDecisionKind kind = SessionDecisionKind::Reject;
    quint8 reason = 0;
    quint64 sequence = 0;
    qint64 expiresAt = 0;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionDecisionEnvelope(const QByteArray &message,
                                   const SessionRequestFields &request,
                                   const QByteArray &signedRequestPayload,
                                   DecodedSessionDecision &decision,
                                   QString &error);

}  // namespace easydesk::client::signaling
