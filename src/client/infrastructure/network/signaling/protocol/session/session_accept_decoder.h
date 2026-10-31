#pragma once

#include "network/signaling/protocol/session/session_accept_codec.h"

namespace easydesk::client::signaling {

struct DecodedSessionAccept {
    SessionAcceptFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionAcceptEnvelope(const QByteArray &message,
                                 const SessionRequestFields &request,
                                 const QByteArray &signedRequestPayload,
                                 DecodedSessionAccept &accept,
                                 QString &error);

}  // namespace easydesk::client::signaling
