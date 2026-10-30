#pragma once

#include "protocol/session/session_payload.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace easydesk::protocol {

inline constexpr std::size_t kMaximumCandidateBatchSize = 8;

struct SessionCandidate {
    std::string sdpMid;
    std::uint32_t sdpMLineIndex;
    std::string usernameFragment;
    std::string candidate;

    bool operator==(const SessionCandidate&) const = default;
};

struct SessionCandidatesPayload {
    SessionDigest descriptionDigest;
    std::uint32_t iceGeneration;
    std::vector<SessionCandidate> candidates;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

struct SessionGatheringCompletePayload {
    SessionDigest descriptionDigest;
    std::uint32_t iceGeneration;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

std::optional<std::vector<std::uint8_t>> encodeSessionCandidatesPayload(
    const SessionCandidatesPayload& payload);
std::optional<SessionCandidatesPayload> parseSessionCandidatesPayload(
    std::span<const std::uint8_t> signedPayload);
std::optional<std::vector<std::uint8_t>> encodeSessionGatheringCompletePayload(
    const SessionGatheringCompletePayload& payload);
std::optional<SessionGatheringCompletePayload> parseSessionGatheringCompletePayload(
    std::span<const std::uint8_t> signedPayload);

}  // namespace easydesk::protocol
