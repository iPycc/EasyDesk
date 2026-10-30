#include "protocol/session/session_request.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kMessageType = "session.request";

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

bool isKnownRole(SessionParticipantRole role) {
    return role == SessionParticipantRole::controller ||
           role == SessionParticipantRole::controlled;
}

}  // namespace

bool validateSessionRequestPayload(const SessionRequestPayload& payload) {
    if (!isKnownRole(payload.requesterRole) ||
        payload.requesterPublicKey == payload.targetPublicKey || payload.expiresAt <= 0)
        return false;
    return validateCapabilityGrants(payload.requestedGrants,
                                    requestControllerPublicKey(payload),
                                    requestControlledPublicKey(payload));
}

const SessionPublicKey& requestControllerPublicKey(const SessionRequestPayload& payload) {
    return payload.requesterRole == SessionParticipantRole::controller
               ? payload.requesterPublicKey
               : payload.targetPublicKey;
}

const SessionPublicKey& requestControlledPublicKey(const SessionRequestPayload& payload) {
    return payload.requesterRole == SessionParticipantRole::controlled
               ? payload.requesterPublicKey
               : payload.targetPublicKey;
}

std::optional<std::vector<std::uint8_t>> encodeSessionRequestPayload(
    const SessionRequestPayload& payload) {
    if (!validateSessionRequestPayload(payload))
        return std::nullopt;

    cbor::Writer writer;
    writer.beginArray(10);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kMessageType);
    writer.writeBytes(payload.requestId);
    writer.writeBytes(payload.sessionId);
    writer.writeBytes(payload.requesterPublicKey);
    writer.writeBytes(payload.targetPublicKey);
    writer.writeUnsigned(static_cast<std::uint8_t>(payload.requesterRole));
    writer.writeBytes(payload.nonceA);
    writeCapabilityGrants(writer, payload.requestedGrants);
    writer.writeUnsigned(static_cast<std::uint64_t>(payload.expiresAt));
    return writer.takeBytes();
}

std::optional<SessionRequestPayload> parseSessionRequestPayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto messageType = reader.readText(kMessageType.size());
    const auto requestId = readFixedBytes<kSessionIdentifierSize>(reader);
    const auto sessionId = readFixedBytes<kSessionIdentifierSize>(reader);
    const auto requesterPublicKey = readFixedBytes<kSessionPublicKeySize>(reader);
    const auto targetPublicKey = readFixedBytes<kSessionPublicKeySize>(reader);
    const auto roleValue = reader.readUnsigned();
    const auto nonceA = readFixedBytes<kSessionNonceSize>(reader);
    if (!arraySize || *arraySize != 10 || !domain || *domain != kSessionSigningDomain ||
        !messageType || *messageType != kMessageType || !requestId || !sessionId ||
        !requesterPublicKey || !targetPublicKey || !roleValue ||
        *roleValue < static_cast<std::uint8_t>(SessionParticipantRole::controller) ||
        *roleValue > static_cast<std::uint8_t>(SessionParticipantRole::controlled) || !nonceA) {
        return std::nullopt;
    }

    const auto requesterRole = static_cast<SessionParticipantRole>(*roleValue);
    const SessionPublicKey& controller = requesterRole == SessionParticipantRole::controller
                                             ? *requesterPublicKey
                                             : *targetPublicKey;
    const SessionPublicKey& controlled = requesterRole == SessionParticipantRole::controlled
                                             ? *requesterPublicKey
                                             : *targetPublicKey;
    auto requestedGrants = readCapabilityGrants(reader, controller, controlled);
    const auto expiresAt = reader.readUnsigned();
    if (!requestedGrants || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished()) {
        return std::nullopt;
    }

    SessionRequestPayload payload{
        .requestId = *requestId,
        .sessionId = *sessionId,
        .requesterPublicKey = *requesterPublicKey,
        .targetPublicKey = *targetPublicKey,
        .requesterRole = requesterRole,
        .nonceA = *nonceA,
        .requestedGrants = std::move(*requestedGrants),
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
    return validateSessionRequestPayload(payload)
               ? std::optional<SessionRequestPayload>(std::move(payload))
               : std::nullopt;
}

}  // namespace easydesk::protocol
