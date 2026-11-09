#include "transport/net/websocket_session.hpp"

#include "application/hub/signaling_hub.hpp"
#include "application/turn/turn_credential_issuer.hpp"
#include "protocol/encoding/base64url.hpp"
#include "protocol/session/session_envelope.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <string>
#include <utility>

namespace easydesk::net {
namespace {

boost::json::object credentialMessage(const protocol::SessionEnvelope& accept,
                                      const turn::TurnCredentials& credentials) {
    boost::json::array urls;
    urls.reserve(credentials.urls.size());
    for (const auto& url : credentials.urls)
        urls.emplace_back(url);

    boost::json::object message;
    message["type"] = "turn.credentials";
    message["protocol"] = protocol::kSessionProtocol;
    message["request_id"] = protocol::encodeBase64Url(accept.requestId);
    message["session_id"] = protocol::encodeBase64Url(accept.sessionId);
    message["session_epoch"] = *accept.sessionEpoch;
    message["urls"] = std::move(urls);
    message["username"] = credentials.username;
    message["password"] = credentials.password;
    message["expires_at"] = credentials.expiresAt;
    return message;
}

boost::json::object unavailableMessage(const protocol::SessionEnvelope& accept) {
    boost::json::object message;
    message["type"] = "turn.unavailable";
    message["protocol"] = protocol::kSessionProtocol;
    message["request_id"] = protocol::encodeBase64Url(accept.requestId);
    message["session_id"] = protocol::encodeBase64Url(accept.sessionId);
    message["session_epoch"] = *accept.sessionEpoch;
    return message;
}

}  // namespace

void WebSocketSession::sendTurnCredentials(const protocol::SessionEnvelope& accept) {
    if (!accept.sessionEpoch)
        return;
    if (turnCredentials_ == nullptr) {
        sendJson(unavailableMessage(accept));
        (void)hub_.route(deviceId_, this, accept.toDeviceId, unavailableMessage(accept));
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto local = turnCredentials_->issue(deviceId_, accept.sessionId, now);
    const auto remote = turnCredentials_->issue(accept.toDeviceId, accept.sessionId, now);
    if (!local || !remote) {
        sendError("E_TURN_CONFIG", "TURN credentials could not be issued", false);
        return;
    }

    sendJson(credentialMessage(accept, *local));
    (void)hub_.route(deviceId_, this, accept.toDeviceId,
                     credentialMessage(accept, *remote));
}

}  // namespace easydesk::net
