#include "protocol/session/session_envelope_json.hpp"

#include "protocol/encoding/base64url.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace easydesk::protocol {
namespace {

std::optional<std::string> stringField(const boost::json::object& object,
                                       std::string_view name,
                                       std::size_t maximumBytes) {
    const auto* value = object.if_contains(name);
    if (value == nullptr || !value->is_string())
        return std::nullopt;
    const auto& text = value->as_string();
    if (text.empty() || text.size() > maximumBytes)
        return std::nullopt;
    return std::string(text.data(), text.size());
}

std::optional<std::uint64_t> unsignedInteger(const boost::json::value* value,
                                             bool allowZero) {
    if (value == nullptr)
        return std::nullopt;
    std::uint64_t number = 0;
    if (value->is_uint64()) {
        number = value->as_uint64();
    } else if (value->is_int64() && value->as_int64() >= 0) {
        number = static_cast<std::uint64_t>(value->as_int64());
    } else {
        return std::nullopt;
    }
    return allowZero || number > 0 ? std::optional<std::uint64_t>(number) : std::nullopt;
}

bool isDeviceId(std::string_view value) {
    return value.size() == 9 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

std::optional<std::array<std::uint8_t, kSessionIdentifierSize>> identifier(
    const std::string& text) {
    auto decoded = decodeBase64Url(text);
    if (!decoded || decoded->size() != kSessionIdentifierSize ||
        encodeBase64Url(*decoded) != text) {
        return std::nullopt;
    }
    std::array<std::uint8_t, kSessionIdentifierSize> result{};
    std::copy(decoded->begin(), decoded->end(), result.begin());
    return result;
}

std::optional<std::vector<std::uint8_t>> canonicalBase64(const std::string& text,
                                                         std::size_t maximumBytes) {
    auto decoded = decodeBase64Url(text);
    if (!decoded || decoded->empty() || decoded->size() > maximumBytes ||
        encodeBase64Url(*decoded) != text) {
        return std::nullopt;
    }
    return decoded;
}

ProtocolError invalid(std::string message) {
    return {"E_INVALID_SESSION_MESSAGE", std::move(message)};
}

bool hasOnlyEnvelopeFields(const boost::json::object& object) {
    constexpr std::array<std::string_view, 12> allowed{
        "protocol",       "type",           "request_id",     "session_id",
        "session_epoch",  "ice_generation", "seq",            "expires_at",
        "from_device_id", "to_device_id",   "signed_payload", "signature",
    };
    return std::all_of(object.begin(), object.end(), [&allowed](const auto& field) {
        const std::string_view name(field.key().data(), field.key().size());
        return std::find(allowed.begin(), allowed.end(), name) != allowed.end();
    });
}

}  // namespace

SessionEnvelopeParseResult parseSessionEnvelope(const boost::json::object& object) {
    if (!hasOnlyEnvelopeFields(object))
        return invalid("session envelope contains an unsupported field");

    const auto protocol = stringField(object, "protocol", 32);
    const auto typeText = stringField(object, "type", 48);
    const auto requestIdText = stringField(object, "request_id", 32);
    const auto sessionIdText = stringField(object, "session_id", 32);
    const auto fromDeviceId = stringField(object, "from_device_id", 9);
    const auto toDeviceId = stringField(object, "to_device_id", 9);
    const auto payloadText = stringField(object, "signed_payload", 60 * 1024);
    const auto signatureText = stringField(object, "signature", 128);
    const auto sessionEpoch = unsignedInteger(object.if_contains("session_epoch"), false);
    const auto sequence = unsignedInteger(object.if_contains("seq"), false);
    const auto expiresAt = unsignedInteger(object.if_contains("expires_at"), false);

    if (!protocol || *protocol != kSessionProtocol || !typeText || !requestIdText ||
        !sessionIdText || !fromDeviceId || !toDeviceId || !payloadText || !signatureText ||
        !sessionEpoch || *sessionEpoch > std::numeric_limits<std::uint32_t>::max() ||
        !sequence || !expiresAt || *expiresAt > std::numeric_limits<std::int64_t>::max()) {
        return invalid("session envelope fields are missing or outside their allowed range");
    }

    const auto type = parseSessionMessageType(*typeText);
    const auto requestId = identifier(*requestIdText);
    const auto sessionId = identifier(*sessionIdText);
    auto signedPayload = canonicalBase64(*payloadText, kSessionPayloadMaximumBytes);
    auto signature = canonicalBase64(*signatureText, 64);
    if (!type || !requestId || !sessionId || !isDeviceId(*fromDeviceId) ||
        !isDeviceId(*toDeviceId) || *fromDeviceId == *toDeviceId || !signedPayload ||
        !signature || signature->size() != 64) {
        return invalid("session envelope identity, type, payload, or signature is invalid");
    }

    const auto* iceValue = object.if_contains("ice_generation");
    std::optional<std::uint32_t> iceGeneration;
    if (sessionMessageRequiresIceGeneration(*type)) {
        const auto parsed = unsignedInteger(iceValue, true);
        if (!parsed || *parsed > std::numeric_limits<std::uint32_t>::max())
            return invalid("this session message requires a valid ICE generation");
        iceGeneration = static_cast<std::uint32_t>(*parsed);
    } else if (iceValue != nullptr) {
        return invalid("this session message must not carry an ICE generation");
    }

    return SessionEnvelope{
        .type = *type,
        .requestId = *requestId,
        .sessionId = *sessionId,
        .sessionEpoch = static_cast<std::uint32_t>(*sessionEpoch),
        .iceGeneration = iceGeneration,
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
        .fromDeviceId = *fromDeviceId,
        .toDeviceId = *toDeviceId,
        .signedPayload = std::move(*signedPayload),
        .signature = std::move(*signature),
    };
}

boost::json::object sessionEnvelopeToJson(const SessionEnvelope& envelope) {
    boost::json::object object;
    object["protocol"] = kSessionProtocol;
    object["type"] = sessionMessageTypeName(envelope.type);
    object["request_id"] = encodeBase64Url(envelope.requestId);
    object["session_id"] = encodeBase64Url(envelope.sessionId);
    object["session_epoch"] = envelope.sessionEpoch;
    if (envelope.iceGeneration)
        object["ice_generation"] = *envelope.iceGeneration;
    object["seq"] = envelope.sequence;
    object["expires_at"] = envelope.expiresAt;
    object["from_device_id"] = envelope.fromDeviceId;
    object["to_device_id"] = envelope.toDeviceId;
    object["signed_payload"] = encodeBase64Url(envelope.signedPayload);
    object["signature"] = encodeBase64Url(envelope.signature);
    return object;
}

}  // namespace easydesk::protocol
