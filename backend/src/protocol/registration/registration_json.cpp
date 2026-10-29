#include "protocol/registration/registration_json.hpp"

#include "protocol/encoding/base64url.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

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

ProtocolError invalid(std::string message) {
    return {"E_INVALID_MESSAGE", std::move(message)};
}

}  // namespace

RegistrationParseResult parseRegistrationResponse(const boost::json::object& object) {
    const auto deviceId = stringField(object, "device_id", 9);
    const auto publicKeyText = stringField(object, "public_key", 64);
    const auto displayName = stringField(object, "display_name", 128);
    const auto platform = stringField(object, "platform", 64);
    const auto nonce = stringField(object, "nonce", 128);
    const auto signatureText = stringField(object, "signature", 128);
    const auto* schemaValue = object.if_contains("schema_version");
    const auto* timestampValue = object.if_contains("timestamp");

    if (!deviceId || deviceId->size() != 9 || !publicKeyText || !displayName || !platform ||
        !nonce || !signatureText || schemaValue == nullptr || !schemaValue->is_int64() ||
        timestampValue == nullptr || !timestampValue->is_int64()) {
        return invalid("registration fields are missing or invalid");
    }

    const auto schemaVersion = schemaValue->as_int64();
    const auto timestamp = timestampValue->as_int64();
    if (schemaVersion < 1 || schemaVersion > 65535 || timestamp <= 0) {
        return invalid("registration numbers are outside their allowed range");
    }

    auto publicKey = decodeBase64Url(*publicKeyText);
    auto signature = decodeBase64Url(*signatureText);
    if (!publicKey || publicKey->size() != 32 || !signature || signature->size() != 64) {
        return invalid("registration key or signature has an invalid encoding");
    }

    return RegistrationEnvelope{
        .deviceId = *deviceId,
        .publicKey = std::move(*publicKey),
        .displayName = *displayName,
        .platform = *platform,
        .schemaVersion = static_cast<std::uint32_t>(schemaVersion),
        .timestamp = timestamp,
        .nonce = *nonce,
        .signature = std::move(*signature),
    };
}

}  // namespace easydesk::protocol
