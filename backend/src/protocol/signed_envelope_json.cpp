#include "protocol/signed_envelope_json.hpp"

#include "protocol/base64url.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace easydesk::protocol {
namespace {

std::optional<std::string> stringField(const boost::json::object& object,
                                       std::string_view name,
                                       std::size_t maximumBytes) {
    const auto* value = object.if_contains(name);
    if (value == nullptr || !value->is_string()) {
        return std::nullopt;
    }
    const auto& text = value->as_string();
    if (text.empty() || text.size() > maximumBytes) {
        return std::nullopt;
    }
    return std::string(text.data(), text.size());
}

std::optional<std::uint64_t> positiveInteger(const boost::json::value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (value->is_uint64() && value->as_uint64() > 0) {
        return value->as_uint64();
    }
    if (value->is_int64() && value->as_int64() > 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    return std::nullopt;
}

ProtocolError invalid(std::string message) {
    return {"E_INVALID_MESSAGE", std::move(message)};
}

}  // namespace

EnvelopeParseResult parseSignedEnvelope(const boost::json::object& object) {
    const auto type = stringField(object, "type", 64);
    const auto targetId = stringField(object, "target_id", 9);
    const auto sessionId = stringField(object, "session_id", 64);
    const auto payloadText = stringField(object, "payload", 90 * 1024);
    const auto signatureText = stringField(object, "signature", 128);
    const auto epoch = positiveInteger(object.if_contains("session_epoch"));
    const auto sequence = positiveInteger(object.if_contains("sequence"));
    const auto expiresAt = positiveInteger(object.if_contains("expires_at"));

    if (!type || !targetId || targetId->size() != 9 || !sessionId || !payloadText ||
        !signatureText || !epoch || *epoch > std::numeric_limits<std::uint32_t>::max() ||
        !sequence || !expiresAt || *expiresAt > std::numeric_limits<std::int64_t>::max()) {
        return invalid("signed envelope fields are missing or invalid");
    }

    auto payload = decodeBase64Url(*payloadText);
    auto signature = decodeBase64Url(*signatureText);
    if (!payload || payload->size() > 64 * 1024 || !signature || signature->size() != 64) {
        return invalid("signed envelope payload or signature has an invalid encoding");
    }

    return SignedEnvelope{
        .type = *type,
        .targetId = *targetId,
        .sessionId = *sessionId,
        .sessionEpoch = static_cast<std::uint32_t>(*epoch),
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
        .payload = std::move(*payload),
        .signature = std::move(*signature),
    };
}

boost::json::object signedEnvelopeToJson(const SignedEnvelope& envelope,
                                         const std::string& sourceDeviceId) {
    boost::json::object object;
    object["type"] = envelope.type;
    object["from_device_id"] = sourceDeviceId;
    object["target_id"] = envelope.targetId;
    object["session_id"] = envelope.sessionId;
    object["session_epoch"] = envelope.sessionEpoch;
    object["sequence"] = envelope.sequence;
    object["expires_at"] = envelope.expiresAt;
    object["payload"] = encodeBase64Url(envelope.payload);
    object["signature"] = encodeBase64Url(envelope.signature);
    return object;
}

}  // namespace easydesk::protocol
