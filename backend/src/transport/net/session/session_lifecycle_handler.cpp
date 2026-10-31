#include "transport/net/websocket_session.hpp"

#include "application/hub/signaling_hub.hpp"
#include "application/session/session_registry.hpp"
#include "protocol/session/session_envelope_json.hpp"
#include "protocol/session/session_verifier.hpp"

#include <chrono>
#include <cstdint>
#include <utility>
#include <variant>

namespace easydesk::net {
namespace {

constexpr auto kDecisionLifetime = std::chrono::seconds(60);

std::int64_t unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

void WebSocketSession::handleSessionDecision(
    const boost::json::object& object,
    protocol::SessionMessageType expectedType) {
    auto parsed = protocol::parseSessionEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SessionEnvelope>(std::move(parsed));
    if (envelope.type != expectedType ||
        (expectedType != protocol::SessionMessageType::reject &&
         expectedType != protocol::SessionMessageType::cancel) ||
        envelope.fromDeviceId != deviceId_) {
        sendError("E_INVALID_ROUTE", "session decision route is invalid", false);
        return;
    }

    const std::int64_t nowUnix = unixSeconds();
    if (envelope.expiresAt < nowUnix ||
        envelope.expiresAt > nowUnix + kDecisionLifetime.count()) {
        sendError("E_EXPIRED", "session decision is expired or too far in the future", false);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto request = expectedType == protocol::SessionMessageType::reject
                             ? sessions_.findPendingForResponder(
                                   envelope.requestId, envelope.sessionId, deviceId_, now)
                             : sessions_.findPendingForRequester(
                                   envelope.requestId, envelope.sessionId, deviceId_, now);
    if (!request) {
        sendError("E_SESSION_NOT_PENDING", "session request is no longer pending", false);
        return;
    }

    if (expectedType == protocol::SessionMessageType::reject) {
        auto verified = protocol::verifySessionReject(envelope, *request);
        if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
            sendError(*error == protocol::SessionVerificationError::invalidSignature
                          ? "E_SIGNATURE"
                          : "E_INVALID_SESSION_MESSAGE",
                      "session rejection authentication failed", false);
            return;
        }
    } else {
        auto verified = protocol::verifySessionCancel(envelope, *request);
        if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
            sendError(*error == protocol::SessionVerificationError::invalidSignature
                          ? "E_SIGNATURE"
                          : "E_INVALID_SESSION_MESSAGE",
                      "session cancellation authentication failed", false);
            return;
        }
    }

    if (!sessions_.consumePending(envelope.requestId, envelope.sessionId, deviceId_, now)) {
        sendError("E_SESSION_NOT_PENDING", "session request is no longer pending", false);
        return;
    }
    const auto routeResult =
        hub_.route(deviceId_, this, envelope.toDeviceId, protocol::sessionEnvelopeToJson(envelope));
    if (routeResult != hub::RouteResult::delivered) {
        sendError(routeResult == hub::RouteResult::senderMissing ? "E_CONNECTION_STATE"
                                                                 : "E_TARGET_OFFLINE",
                  "session decision could not be delivered",
                  routeResult == hub::RouteResult::senderMissing);
        return;
    }
    sendSessionDelivered(envelope);
}

void WebSocketSession::handleSessionEnd(const boost::json::object& object) {
    auto parsed = protocol::parseSessionEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SessionEnvelope>(std::move(parsed));
    if (envelope.type != protocol::SessionMessageType::end ||
        envelope.fromDeviceId != deviceId_) {
        sendError("E_INVALID_ROUTE", "session end route is invalid", false);
        return;
    }

    const std::int64_t nowUnix = unixSeconds();
    if (envelope.expiresAt < nowUnix ||
        envelope.expiresAt > nowUnix + kDecisionLifetime.count()) {
        sendError("E_EXPIRED", "session end is expired or too far in the future", false);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto active = sessions_.findActiveForParticipant(
        envelope.requestId, envelope.sessionId, deviceId_, now);
    if (!active) {
        sendError("E_SESSION_NOT_ACTIVE", "session is not active", false);
        return;
    }

    auto verified = protocol::verifySessionEnd(
        envelope, active->request, active->accept,
        active->revoke ? &*active->revoke : nullptr);
    if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
        sendError(*error == protocol::SessionVerificationError::invalidSignature
                      ? "E_SIGNATURE"
                      : "E_INVALID_SESSION_MESSAGE",
                  "session end authentication failed", false);
        return;
    }
    const auto ended = sessions_.endSession(
        std::get<protocol::VerifiedSessionEnd>(verified), now);
    if (ended == session::SessionTerminationResult::replay) {
        sendError("E_REPLAY", "session end sequence was already processed", false);
        return;
    }
    if (ended == session::SessionTerminationResult::staleAuthorization) {
        sendError("E_PERMISSION_EPOCH", "session authorization has changed", false);
        return;
    }
    if (ended != session::SessionTerminationResult::ended) {
        sendError("E_SESSION_NOT_ACTIVE", "session is not active", false);
        return;
    }

    const auto routeResult =
        hub_.route(deviceId_, this, envelope.toDeviceId, protocol::sessionEnvelopeToJson(envelope));
    if (routeResult != hub::RouteResult::delivered) {
        sendError(routeResult == hub::RouteResult::senderMissing ? "E_CONNECTION_STATE"
                                                                 : "E_TARGET_OFFLINE",
                  "session end could not be delivered",
                  routeResult == hub::RouteResult::senderMissing);
        return;
    }
    sendSessionDelivered(envelope);
}

void WebSocketSession::handleSessionRevoke(const boost::json::object& object) {
    auto parsed = protocol::parseSessionEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SessionEnvelope>(std::move(parsed));
    if (envelope.type != protocol::SessionMessageType::revoke ||
        envelope.fromDeviceId != deviceId_) {
        sendError("E_INVALID_ROUTE", "session revoke route is invalid", false);
        return;
    }

    const std::int64_t nowUnix = unixSeconds();
    if (envelope.expiresAt < nowUnix ||
        envelope.expiresAt > nowUnix + kDecisionLifetime.count()) {
        sendError("E_EXPIRED", "session revoke is expired or too far in the future", false);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto active = sessions_.findActiveForParticipant(
        envelope.requestId, envelope.sessionId, deviceId_, now);
    if (!active) {
        sendError("E_SESSION_NOT_ACTIVE", "session is not active", false);
        return;
    }

    auto verified = protocol::verifySessionRevoke(
        envelope, active->request, active->accept,
        active->revoke ? &*active->revoke : nullptr);
    if (const auto* error = std::get_if<protocol::SessionVerificationError>(&verified)) {
        sendError(*error == protocol::SessionVerificationError::invalidSignature
                      ? "E_SIGNATURE"
                      : "E_INVALID_SESSION_MESSAGE",
                  "session revoke authentication failed", false);
        return;
    }
    const auto stored = sessions_.storeRevoke(
        std::get<protocol::VerifiedSessionRevoke>(std::move(verified)), now);
    if (stored == session::PermissionStoreResult::replay) {
        sendError("E_REPLAY", "session revoke sequence was already processed", false);
        return;
    }
    if (stored == session::PermissionStoreResult::staleAuthorization) {
        sendError("E_PERMISSION_EPOCH", "session authorization has changed", false);
        return;
    }
    if (stored != session::PermissionStoreResult::stored) {
        sendError("E_SESSION_NOT_ACTIVE", "session is not active", false);
        return;
    }

    const auto routeResult =
        hub_.route(deviceId_, this, envelope.toDeviceId, protocol::sessionEnvelopeToJson(envelope));
    if (routeResult != hub::RouteResult::delivered) {
        sessions_.removeSession(envelope.requestId, envelope.sessionId, deviceId_);
        sendError(routeResult == hub::RouteResult::senderMissing ? "E_CONNECTION_STATE"
                                                                 : "E_TARGET_OFFLINE",
                  "session revoke could not be delivered",
                  routeResult == hub::RouteResult::senderMissing);
        return;
    }
    sendSessionDelivered(envelope);
}

}  // namespace easydesk::net
