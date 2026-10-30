#pragma once

#include "protocol/session/session_payload.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace easydesk::protocol {

enum class SessionRejectReason : std::uint8_t {
    declined = 1,
    unavailable = 2,
};

enum class SessionCancelReason : std::uint8_t {
    userCancelled = 1,
    negotiationFailed = 2,
};

struct SessionRejectPayload {
    SessionDigest requestDigest;
    SessionRejectReason reason;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

struct SessionCancelPayload {
    SessionDigest requestDigest;
    SessionCancelReason reason;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

std::optional<std::vector<std::uint8_t>> encodeSessionRejectPayload(
    const SessionRejectPayload& payload);
std::optional<SessionRejectPayload> parseSessionRejectPayload(
    std::span<const std::uint8_t> signedPayload);
std::optional<std::vector<std::uint8_t>> encodeSessionCancelPayload(
    const SessionCancelPayload& payload);
std::optional<SessionCancelPayload> parseSessionCancelPayload(
    std::span<const std::uint8_t> signedPayload);

}  // namespace easydesk::protocol
