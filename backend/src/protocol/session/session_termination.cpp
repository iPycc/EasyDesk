#include "protocol/session/session_termination.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kEndType = "session.end";

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

bool validReason(SessionEndReason reason) {
    return reason == SessionEndReason::userStopped ||
           reason == SessionEndReason::connectionFailed ||
           reason == SessionEndReason::authorizationExpired ||
           reason == SessionEndReason::protocolFailure;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> encodeSessionEndPayload(
    const SessionEndPayload& payload) {
    if (payload.sessionEpoch == 0 || !validReason(payload.reason) ||
        payload.sequence == 0 || payload.expiresAt <= 0) {
        return std::nullopt;
    }
    cbor::Writer writer;
    writer.beginArray(7);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kEndType);
    writer.writeBytes(payload.authorizationDigest);
    writer.writeUnsigned(payload.sessionEpoch);
    writer.writeUnsigned(static_cast<std::uint8_t>(payload.reason));
    writer.writeUnsigned(payload.sequence);
    writer.writeUnsigned(static_cast<std::uint64_t>(payload.expiresAt));
    return writer.takeBytes();
}

std::optional<SessionEndPayload> parseSessionEndPayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(kEndType.size());
    const auto authorizationDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto sessionEpoch = reader.readUnsigned();
    const auto reasonValue = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 7 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != kEndType || !authorizationDigest || !sessionEpoch ||
        *sessionEpoch == 0 || *sessionEpoch > std::numeric_limits<std::uint32_t>::max() ||
        !reasonValue || *reasonValue > std::numeric_limits<std::uint8_t>::max() ||
        !sequence || *sequence == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished()) {
        return std::nullopt;
    }
    const auto reason = static_cast<SessionEndReason>(*reasonValue);
    if (!validReason(reason))
        return std::nullopt;
    return SessionEndPayload{
        .authorizationDigest = *authorizationDigest,
        .sessionEpoch = static_cast<std::uint32_t>(*sessionEpoch),
        .reason = reason,
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
}

}  // namespace easydesk::protocol
