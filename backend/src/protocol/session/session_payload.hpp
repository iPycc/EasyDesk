#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace easydesk::protocol {

inline constexpr std::string_view kSessionSigningDomain = "EasyDesk-Session-v2";
inline constexpr std::size_t kSessionNonceSize = 32;
inline constexpr std::size_t kSessionDigestSize = 32;

using SessionNonce = std::array<std::uint8_t, kSessionNonceSize>;
using SessionDigest = std::array<std::uint8_t, kSessionDigestSize>;

enum class SessionParticipantRole : std::uint8_t {
    controller = 1,
    controlled = 2,
};

SessionDigest sessionPayloadDigest(std::span<const std::uint8_t> signedPayload);

}  // namespace easydesk::protocol
