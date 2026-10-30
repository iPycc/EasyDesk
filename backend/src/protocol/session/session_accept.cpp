#include "protocol/session/session_accept.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kMessageType = "session.accept";

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

std::optional<SessionDigest> requestDigest(const SessionRequestPayload& request) {
    const auto encoded = encodeSessionRequestPayload(request);
    return encoded ? std::optional<SessionDigest>(sessionPayloadDigest(*encoded)) : std::nullopt;
}

}  // namespace

bool validateSessionAcceptPayload(const SessionAcceptPayload& payload,
                                  const SessionRequestPayload& request) {
    const auto expectedDigest = requestDigest(request);
    return expectedDigest && payload.requestDigest == *expectedDigest &&
           payload.sessionEpoch > 0 && payload.permissionEpoch > 0 && payload.expiresAt > 0 &&
           validateCapabilityGrants(payload.grantedGrants,
                                    requestControllerPublicKey(request),
                                    requestControlledPublicKey(request)) &&
           capabilityGrantsAreSubset(payload.grantedGrants, request.requestedGrants);
}

std::optional<std::vector<std::uint8_t>> encodeSessionAcceptPayload(
    const SessionAcceptPayload& payload,
    const SessionRequestPayload& request) {
    if (!validateSessionAcceptPayload(payload, request))
        return std::nullopt;

    cbor::Writer writer;
    writer.beginArray(8);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kMessageType);
    writer.writeBytes(payload.requestDigest);
    writer.writeBytes(payload.nonceB);
    writeCapabilityGrants(writer, payload.grantedGrants);
    writer.writeUnsigned(payload.sessionEpoch);
    writer.writeUnsigned(payload.permissionEpoch);
    writer.writeUnsigned(static_cast<std::uint64_t>(payload.expiresAt));
    return writer.takeBytes();
}

std::optional<SessionAcceptPayload> parseSessionAcceptPayload(
    std::span<const std::uint8_t> signedPayload,
    const SessionRequestPayload& request) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto messageType = reader.readText(kMessageType.size());
    const auto requestDigestValue = readFixedBytes<kSessionDigestSize>(reader);
    const auto nonceB = readFixedBytes<kSessionNonceSize>(reader);
    if (!arraySize || *arraySize != 8 || !domain || *domain != kSessionSigningDomain ||
        !messageType || *messageType != kMessageType || !requestDigestValue || !nonceB) {
        return std::nullopt;
    }

    auto grantedGrants = readCapabilityGrants(reader,
                                              requestControllerPublicKey(request),
                                              requestControlledPublicKey(request));
    const auto sessionEpoch = reader.readUnsigned();
    const auto permissionEpoch = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!grantedGrants || !sessionEpoch || *sessionEpoch == 0 ||
        *sessionEpoch > std::numeric_limits<std::uint32_t>::max() || !permissionEpoch ||
        *permissionEpoch == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished()) {
        return std::nullopt;
    }

    SessionAcceptPayload payload{
        .requestDigest = *requestDigestValue,
        .nonceB = *nonceB,
        .grantedGrants = std::move(*grantedGrants),
        .sessionEpoch = static_cast<std::uint32_t>(*sessionEpoch),
        .permissionEpoch = *permissionEpoch,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
    return validateSessionAcceptPayload(payload, request)
               ? std::optional<SessionAcceptPayload>(std::move(payload))
               : std::nullopt;
}

}  // namespace easydesk::protocol
