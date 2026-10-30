#pragma once

#include "protocol/session/session_payload.hpp"
#include "protocol/session/session_request.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace easydesk::protocol {

struct SessionAcceptPayload {
    SessionDigest requestDigest;
    SessionNonce nonceB;
    std::vector<CapabilityGrant> grantedGrants;
    std::uint32_t sessionEpoch;
    std::uint64_t permissionEpoch;
    std::int64_t expiresAt;
};

bool validateSessionAcceptPayload(const SessionAcceptPayload& payload,
                                  const SessionRequestPayload& request);
std::optional<std::vector<std::uint8_t>> encodeSessionAcceptPayload(
    const SessionAcceptPayload& payload,
    const SessionRequestPayload& request);
std::optional<SessionAcceptPayload> parseSessionAcceptPayload(
    std::span<const std::uint8_t> signedPayload,
    const SessionRequestPayload& request);

}  // namespace easydesk::protocol
