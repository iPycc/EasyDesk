#pragma once

#include "network/signaling/protocol/session/session_request_codec.h"

namespace easydesk::client::signaling {

struct DecodedSessionRequest {
    SessionRequestFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionRequestEnvelope(const QByteArray &message,
                                  DecodedSessionRequest &request,
                                  QString &error);

}  // namespace easydesk::client::signaling
