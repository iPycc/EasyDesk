#pragma once

#include "protocol/session/session_payload.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace easydesk::protocol {

inline constexpr std::size_t kMaximumSessionDescriptionBytes = 32 * 1024;

struct SessionOfferPayload {
    SessionDigest acceptDigest;
    std::uint32_t iceGeneration;
    SessionNonce nonce;
    std::string sdp;
    SessionDigest dtlsFingerprint;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

struct SessionAnswerPayload {
    SessionDigest offerDigest;
    std::uint32_t iceGeneration;
    std::string sdp;
    SessionDigest dtlsFingerprint;
    std::uint64_t sequence;
    std::int64_t expiresAt;
};

std::optional<std::vector<std::uint8_t>> encodeSessionOfferPayload(
    const SessionOfferPayload& payload);
std::optional<SessionOfferPayload> parseSessionOfferPayload(
    std::span<const std::uint8_t> signedPayload);
std::optional<std::vector<std::uint8_t>> encodeSessionAnswerPayload(
    const SessionAnswerPayload& payload);
std::optional<SessionAnswerPayload> parseSessionAnswerPayload(
    std::span<const std::uint8_t> signedPayload);

}  // namespace easydesk::protocol
