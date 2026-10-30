#pragma once

#include "protocol/session/session_accept.hpp"
#include "protocol/session/session_envelope.hpp"
#include "protocol/session/session_request.hpp"

#include <variant>

namespace easydesk::protocol {

enum class SessionVerificationError {
    wrongMessageType,
    invalidRoute,
    invalidSignature,
    invalidPayload,
    envelopeMismatch,
};

struct VerifiedSessionRequest {
    SessionEnvelope envelope;
    SessionRequestPayload payload;
};

struct VerifiedSessionAccept {
    SessionEnvelope envelope;
    SessionAcceptPayload payload;
};

using SessionRequestVerificationResult =
    std::variant<VerifiedSessionRequest, SessionVerificationError>;
using SessionAcceptVerificationResult =
    std::variant<VerifiedSessionAccept, SessionVerificationError>;

SessionRequestVerificationResult verifySessionRequest(
    const SessionEnvelope& envelope,
    const SessionPublicKey& registeredSourcePublicKey,
    const SessionPublicKey& registeredTargetPublicKey);
SessionAcceptVerificationResult verifySessionAccept(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request);

}  // namespace easydesk::protocol
