#include "transport/net/websocket_session.hpp"

#include "application/hub/signaling_hub.hpp"
#include "application/session/session_attempt_limiter.hpp"
#include "application/session/session_registry.hpp"
#include "infrastructure/devices/device_repository.hpp"
#include "protocol/encoding/base64url.hpp"
#include "protocol/session/session_envelope_json.hpp"
#include "protocol/session/session_verifier.hpp"

#include <boost/json/object.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>
#include <variant>

namespace easydesk::net {
namespace {

constexpr auto kRequestLifetime = std::chrono::seconds(60);
constexpr auto kMaximumSessionLifetime = std::chrono::hours(8);

std::int64_t unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

void WebSocketSession::handleSessionRequest(const boost::json::object& object) {
    auto parsed = protocol::parseSessionEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SessionEnvelope>(std::move(parsed));
    if (envelope.type != protocol::SessionMessageType::request ||
        envelope.fromDeviceId != deviceId_) {
        sendError("E_INVALID_ROUTE", "session request route is invalid", false);
        return;
    }

    const std::int64_t nowUnix = unixSeconds();
    if (envelope.expiresAt < nowUnix ||
        envelope.expiresAt > nowUnix + kRequestLifetime.count()) {
        sendError("E_EXPIRED", "session request is expired or too far in the future", false);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (attemptLimiter_.beginOutgoing(deviceId_, now) != session::AttemptLimitResult::allowed ||
        attemptLimiter_.beginIncoming(envelope.toDeviceId, now) !=
            session::AttemptLimitResult::allowed) {
        sendError("E_RATE_LIMIT", "too many session attempts", false);
        return;
    }
    if (!hub_.isOnline(envelope.toDeviceId)) {
        sendError("E_TARGET_OFFLINE", "the target device is not online", false);
        return;
    }
    const auto targetIdentity = devices_.findIdentity(envelope.toDeviceId);
    if (!targetIdentity || publicKey_.size() != protocol::kSessionPublicKeySize) {
        sendError("E_INVALID_ROUTE", "session request identity is unavailable", false);
        return;
    }

    protocol::SessionPublicKey sourcePublicKey{};
    std::copy(publicKey_.begin(), publicKey_.end(), sourcePublicKey.begin());
    auto verified = protocol::verifySessionRequest(envelope, sourcePublicKey,
                                                   targetIdentity->publicKey);
    if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
        sendError(*error == protocol::SessionVerificationError::invalidSignature
                      ? "E_SIGNATURE"
                      : "E_INVALID_SESSION_MESSAGE",
                  "session request authentication failed", false);
        return;
    }
    auto request = std::get<protocol::VerifiedSessionRequest>(std::move(verified));
    const auto pending = sessions_.addPending(std::move(request), now);
    if (pending == session::PendingRequestResult::duplicate) {
        sendError("E_REPLAY", "session request was already processed", false);
        return;
    }
    if (pending == session::PendingRequestResult::participantBusy) {
        sendError("E_SESSION_BUSY", "one of the session participants is busy", false);
        return;
    }

    const auto routeResult =
        hub_.route(deviceId_, this, envelope.toDeviceId, protocol::sessionEnvelopeToJson(envelope));
    if (routeResult != hub::RouteResult::delivered) {
        sessions_.consumePending(envelope.requestId, envelope.sessionId, deviceId_, now);
        sendError(routeResult == hub::RouteResult::senderMissing ? "E_CONNECTION_STATE"
                                                                 : "E_TARGET_OFFLINE",
                  "session request could not be delivered",
                  routeResult == hub::RouteResult::senderMissing);
        return;
    }

    sendSessionDelivered(envelope);
}

void WebSocketSession::handleSessionAccept(const boost::json::object& object) {
    auto parsed = protocol::parseSessionEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SessionEnvelope>(std::move(parsed));
    if (envelope.type != protocol::SessionMessageType::accept ||
        envelope.fromDeviceId != deviceId_) {
        sendError("E_INVALID_ROUTE", "session accept route is invalid", false);
        return;
    }

    const std::int64_t nowUnix = unixSeconds();
    if (envelope.expiresAt <= nowUnix ||
        envelope.expiresAt >
            nowUnix +
                std::chrono::duration_cast<std::chrono::seconds>(kMaximumSessionLifetime).count()) {
        sendError("E_EXPIRED", "session authorization expiry is invalid", false);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto request = sessions_.findPendingForResponder(
        envelope.requestId, envelope.sessionId, deviceId_, now);
    if (!request) {
        sendError("E_SESSION_NOT_PENDING", "session request is no longer pending", false);
        return;
    }

    auto verified = protocol::verifySessionAccept(envelope, *request);
    if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
        sendError(*error == protocol::SessionVerificationError::invalidSignature
                      ? "E_SIGNATURE"
                      : "E_INVALID_SESSION_MESSAGE",
                  "session accept authentication failed", false);
        return;
    }
    auto accept = std::get<protocol::VerifiedSessionAccept>(std::move(verified));
    const auto activeExpiresAt =
        now + std::chrono::seconds(envelope.expiresAt - nowUnix);
    if (sessions_.activate(std::move(accept), now, activeExpiresAt) !=
        session::SessionActivationResult::activated) {
        sendError("E_SESSION_NOT_PENDING", "session request is no longer pending", false);
        return;
    }

    const auto routeResult =
        hub_.route(deviceId_, this, envelope.toDeviceId, protocol::sessionEnvelopeToJson(envelope));
    if (routeResult != hub::RouteResult::delivered) {
        sessions_.removeSession(envelope.requestId, envelope.sessionId, deviceId_);
        sendError(routeResult == hub::RouteResult::senderMissing ? "E_CONNECTION_STATE"
                                                                 : "E_TARGET_OFFLINE",
                  "session accept could not be delivered",
                  routeResult == hub::RouteResult::senderMissing);
        return;
    }

    sendSessionDelivered(envelope);
    sendAssistCode();
}

void WebSocketSession::sendSessionDelivered(const protocol::SessionEnvelope& envelope) {
    boost::json::object delivered;
    delivered["type"] = "session.delivered";
    delivered["protocol"] = protocol::kSessionProtocol;
    delivered["message_type"] = protocol::sessionMessageTypeName(envelope.type);
    delivered["request_id"] = protocol::encodeBase64Url(envelope.requestId);
    delivered["session_id"] = protocol::encodeBase64Url(envelope.sessionId);
    if (envelope.sessionEpoch)
        delivered["session_epoch"] = *envelope.sessionEpoch;
    delivered["seq"] = envelope.sequence;
    sendJson(std::move(delivered));
}

}  // namespace easydesk::net
