#pragma once

#include "protocol/session/session_request.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace easydesk::protocol {

enum class SessionEndReason : std::uint8_t {
    userStopped = 1,
    connectionFailed = 2,
    authorizationExpired = 3,
    protocolFailure = 4,
};

struct SessionEndPayload {
    SessionDigest authorizationDigest;
    std::uint32_t sessionEpoch;
    SessionEndReason reason;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

struct SessionRevokePayload {
    SessionDigest authorizationDigest;
    std::vector<CapabilityGrant> grantedGrants;
    std::uint32_t sessionEpoch;
    std::uint64_t permissionEpoch;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

std::optional<std::vector<std::uint8_t>> encodeSessionEndPayload(
    const SessionEndPayload& payload);
std::optional<SessionEndPayload> parseSessionEndPayload(
    std::span<const std::uint8_t> signedPayload);
std::optional<std::vector<std::uint8_t>> encodeSessionRevokePayload(
    const SessionRevokePayload& payload,
    const SessionRequestPayload& request);
std::optional<SessionRevokePayload> parseSessionRevokePayload(
    std::span<const std::uint8_t> signedPayload,
    const SessionRequestPayload& request);

}  // namespace easydesk::protocol
