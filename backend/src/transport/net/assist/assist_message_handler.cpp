#include "transport/net/websocket_session.hpp"

#include "application/hub/signaling_hub.hpp"
#include "application/session/assist_code_registry.hpp"
#include "application/session/session_attempt_limiter.hpp"
#include "infrastructure/devices/device_repository.hpp"
#include "protocol/encoding/base64url.hpp"
#include "protocol/session/session_envelope.hpp"
#include "security/crypto/ed25519.hpp"

#include <boost/json/object.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace easydesk::net {
namespace {

std::string jsonString(const boost::json::object& object, std::string_view field) {
    const auto* value = object.if_contains(field);
    if (value == nullptr || !value->is_string())
        return {};
    const auto& text = value->as_string();
    return std::string(text.data(), text.size());
}

bool isAssistCode(std::string_view value) {
    return value.size() == 8 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

}  // namespace

void WebSocketSession::handleAssistRefresh(const boost::json::object& object) {
    if (object.size() != 2 || jsonString(object, "protocol") != protocol::kSessionProtocol) {
        sendError("E_INVALID_MESSAGE", "assist.refresh requires the session/2 protocol", false);
        return;
    }
    sendAssistCode();
}

void WebSocketSession::handleAssistResolve(const boost::json::object& object) {
    if (object.size() != 3 || jsonString(object, "protocol") != protocol::kSessionProtocol) {
        sendError("E_INVALID_MESSAGE", "assist.resolve requires the session/2 protocol", false);
        return;
    }
    if (assistResolveAttempts_ >= 3) {
        sendError("E_AUTH_LIMIT", "assist code attempts are exhausted for this connection", false);
        return;
    }
    ++assistResolveAttempts_;

    const auto now = std::chrono::steady_clock::now();
    const auto outgoing = attemptLimiter_.beginOutgoing(deviceId_, now);
    if (outgoing == session::AttemptLimitResult::cooldown) {
        sendError("E_COOLDOWN", "assist code authentication is temporarily unavailable", false);
        return;
    }
    if (outgoing == session::AttemptLimitResult::rateLimited) {
        sendError("E_RATE_LIMIT", "too many session attempts", false);
        return;
    }

    const std::string code = jsonString(object, "code");
    const auto targetId = isAssistCode(code) ? assistCodes_.resolve(code, now) : std::nullopt;
    if (!targetId || *targetId == deviceId_) {
        attemptLimiter_.recordFailure(deviceId_, now);
        sendError("E_ASSIST_CODE_INVALID", "assist code is invalid or unavailable", false);
        return;
    }
    if (attemptLimiter_.beginIncoming(*targetId, now) != session::AttemptLimitResult::allowed ||
        !hub_.isOnline(*targetId)) {
        sendError("E_ASSIST_CODE_UNAVAILABLE", "assist code is invalid or unavailable", false);
        return;
    }

    const auto targetIdentity = devices_.findIdentity(*targetId);
    if (!targetIdentity) {
        sendError("E_ASSIST_CODE_UNAVAILABLE", "assist code is invalid or unavailable", false);
        return;
    }
    attemptLimiter_.recordSuccess(deviceId_, now);

    boost::json::object response;
    response["type"] = "assist.resolved";
    response["protocol"] = protocol::kSessionProtocol;
    response["device_id"] = targetIdentity->locatorId;
    response["public_key"] = protocol::encodeBase64Url(targetIdentity->publicKey);
    response["fingerprint"] = security::sha256Fingerprint(targetIdentity->publicKey);
    sendJson(std::move(response));
}

void WebSocketSession::sendAssistCode() {
    const auto lease = assistCodes_.issue(deviceId_, std::chrono::steady_clock::now());
    boost::json::object response;
    response["type"] = "assist.code";
    response["protocol"] = protocol::kSessionProtocol;
    response["code"] = lease.code;
    response["expires_in_seconds"] =
        std::chrono::duration_cast<std::chrono::seconds>(session::AssistCodeRegistry::kLifetime)
            .count();
    sendJson(std::move(response));
}

}  // namespace easydesk::net
