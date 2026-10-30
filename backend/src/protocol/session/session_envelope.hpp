#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace easydesk::protocol {

inline constexpr std::string_view kSessionProtocol = "easydesk-session/2";
inline constexpr std::size_t kSessionIdentifierSize = 16;
inline constexpr std::size_t kSessionPayloadMaximumBytes = 44 * 1024;

enum class SessionMessageType {
    request,
    accept,
    reject,
    cancel,
    offer,
    answer,
    candidates,
    gatheringComplete,
    end,
    revoke,
};

struct SessionEnvelope {
    SessionMessageType type;
    std::array<std::uint8_t, kSessionIdentifierSize> requestId;
    std::array<std::uint8_t, kSessionIdentifierSize> sessionId;
    std::uint32_t sessionEpoch;
    std::optional<std::uint32_t> iceGeneration;
    std::uint64_t sequence;
    std::int64_t expiresAt;
    std::string fromDeviceId;
    std::string toDeviceId;
    std::vector<std::uint8_t> signedPayload;
    std::vector<std::uint8_t> signature;
};

std::optional<SessionMessageType> parseSessionMessageType(std::string_view value);
std::string_view sessionMessageTypeName(SessionMessageType type);
bool sessionMessageRequiresIceGeneration(SessionMessageType type);

}  // namespace easydesk::protocol
