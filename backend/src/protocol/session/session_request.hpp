#pragma once

#include "protocol/session/session_capability.hpp"
#include "protocol/session/session_envelope.hpp"
#include "protocol/session/session_payload.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace easydesk::protocol {

struct SessionRequestPayload {
    SessionIdentifier requestId;
    SessionIdentifier sessionId;
    SessionPublicKey requesterPublicKey;
    SessionPublicKey targetPublicKey;
    SessionParticipantRole requesterRole;
    SessionNonce nonceA;
    std::vector<CapabilityGrant> requestedGrants;
    std::int64_t expiresAt;
};

bool validateSessionRequestPayload(const SessionRequestPayload& payload);
const SessionPublicKey& requestControllerPublicKey(const SessionRequestPayload& payload);
const SessionPublicKey& requestControlledPublicKey(const SessionRequestPayload& payload);
std::optional<std::vector<std::uint8_t>> encodeSessionRequestPayload(
    const SessionRequestPayload& payload);
std::optional<SessionRequestPayload> parseSessionRequestPayload(
    std::span<const std::uint8_t> signedPayload);

}  // namespace easydesk::protocol
