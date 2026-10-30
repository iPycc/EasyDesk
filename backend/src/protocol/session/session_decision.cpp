#include "protocol/session/session_decision.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kRejectType = "session.reject";
constexpr std::string_view kCancelType = "session.cancel";

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

bool validReason(SessionRejectReason reason) {
    return reason == SessionRejectReason::declined ||
           reason == SessionRejectReason::unavailable;
}

bool validReason(SessionCancelReason reason) {
    return reason == SessionCancelReason::userCancelled ||
           reason == SessionCancelReason::negotiationFailed;
}

template <typename Reason>
std::optional<std::vector<std::uint8_t>> encodeDecision(
    std::string_view type,
    const SessionDigest& requestDigest,
    Reason reason,
    std::uint64_t sequence,
    std::int64_t expiresAt) {
    if (!validReason(reason) || sequence == 0 || expiresAt <= 0)
        return std::nullopt;
    cbor::Writer writer;
    writer.beginArray(6);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(type);
    writer.writeBytes(requestDigest);
    writer.writeUnsigned(static_cast<std::uint8_t>(reason));
    writer.writeUnsigned(sequence);
    writer.writeUnsigned(static_cast<std::uint64_t>(expiresAt));
    return writer.takeBytes();
}

template <typename Reason, typename Payload>
std::optional<Payload> parseDecision(std::span<const std::uint8_t> signedPayload,
                                     std::string_view expectedType) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(expectedType.size());
    const auto requestDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto reasonValue = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 6 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != expectedType || !requestDigest || !reasonValue ||
        *reasonValue > std::numeric_limits<std::uint8_t>::max() || !sequence ||
        *sequence == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished()) {
        return std::nullopt;
    }

    const auto reason = static_cast<Reason>(*reasonValue);
    if (!validReason(reason))
        return std::nullopt;
    return Payload{
        .requestDigest = *requestDigest,
        .reason = reason,
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
}

}  // namespace

std::optional<std::vector<std::uint8_t>> encodeSessionRejectPayload(
    const SessionRejectPayload& payload) {
    return encodeDecision(kRejectType, payload.requestDigest, payload.reason,
                          payload.sequence, payload.expiresAt);
}

std::optional<SessionRejectPayload> parseSessionRejectPayload(
    std::span<const std::uint8_t> signedPayload) {
    return parseDecision<SessionRejectReason, SessionRejectPayload>(signedPayload, kRejectType);
}

std::optional<std::vector<std::uint8_t>> encodeSessionCancelPayload(
    const SessionCancelPayload& payload) {
    return encodeDecision(kCancelType, payload.requestDigest, payload.reason,
                          payload.sequence, payload.expiresAt);
}

std::optional<SessionCancelPayload> parseSessionCancelPayload(
    std::span<const std::uint8_t> signedPayload) {
    return parseDecision<SessionCancelReason, SessionCancelPayload>(signedPayload, kCancelType);
}

}  // namespace easydesk::protocol
