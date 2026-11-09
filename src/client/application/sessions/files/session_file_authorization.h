#pragma once

#include "network/signaling/protocol/session/session_capability.h"
#include "sessions/negotiation/session_negotiation_context.h"

bool applyFileAuthorization(
    SessionNegotiationContext &context,
    const QVector<easydesk::client::signaling::SessionCapabilityGrant> &grants,
    const QByteArray &localPublicKey, const QByteArray &peerPublicKey);
