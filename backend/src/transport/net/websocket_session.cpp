#include "transport/net/websocket_session.hpp"

#include "application/hub/signaling_hub.hpp"
#include "infrastructure/devices/device_repository.hpp"
#include "protocol/encoding/base64url.hpp"
#include "protocol/registration/registration_json.hpp"
#include "protocol/signaling/signed_envelope_json.hpp"
#include "security/crypto/ed25519.hpp"
#include "security/crypto/random.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <variant>

namespace easydesk::net {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

constexpr std::string_view kProtocol = "easydesk-signaling/1";
constexpr auto kRegistrationTimeout = std::chrono::seconds(5);
constexpr auto kTimestampTolerance = std::chrono::seconds(60);

std::int64_t unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string jsonString(const boost::json::object& object, std::string_view field) {
    const auto* value = object.if_contains(field);
    if (value == nullptr || !value->is_string()) {
        return {};
    }
    const auto& text = value->as_string();
    return std::string(text.data(), text.size());
}

bool isDeviceId(std::string_view value) {
    return value.size() == 9 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

bool isSessionId(std::string_view value) {
    return value.size() >= 16 && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isalnum(character) != 0 || character == '-' || character == '_';
           });
}

bool isPairMessage(std::string_view type) {
    return type == "pair.request" || type == "pair.response" || type == "pair.cancel";
}

}  // namespace

WebSocketSession::WebSocketSession(beast::tcp_stream stream,
                                   devices::DeviceRepository& devices,
                                   hub::SignalingHub& hub)
    : strand_(boost::asio::make_strand(
          static_cast<boost::asio::io_context&>(stream.get_executor().context()))),
      websocket_(std::move(stream)),
      registrationTimer_(strand_),
      devices_(devices),
      hub_(hub) {}

WebSocketSession::~WebSocketSession() {
    if (connectedToHub_) {
        hub_.disconnect(deviceId_, this);
    }
}

void WebSocketSession::accept(Request request) {
    auto sharedRequest = std::make_shared<Request>(std::move(request));
    boost::asio::dispatch(strand_, [self = shared_from_this(), sharedRequest] {
        self->websocket_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        self->websocket_.set_option(websocket::stream_base::decorator([](websocket::response_type& response) {
            response.set(http::field::server, "EasyDesk-Signal/0.1");
        }));
        self->websocket_.read_message_max(64 * 1024);
        self->websocket_.async_accept(
            *sharedRequest,
            boost::asio::bind_executor(
                self->strand_, [self, sharedRequest](beast::error_code error) {
                    self->onAccept(error);
                }));
    });
}

void WebSocketSession::deliver(boost::json::object message) {
    boost::asio::dispatch(strand_, [self = shared_from_this(), message = std::move(message)]() mutable {
        if (!self->closing_) {
            self->sendJson(std::move(message));
        }
    });
}

void WebSocketSession::onAccept(beast::error_code error) {
    if (error) {
        return;
    }
    sendChallenge();
    readMessage();
}

void WebSocketSession::sendChallenge() {
    nonce_ = protocol::encodeBase64Url(security::randomBytes(32));
    challengeExpiresAt_ = unixSeconds() + kRegistrationTimeout.count();

    boost::json::object message;
    message["type"] = "register.challenge";
    message["protocol"] = kProtocol;
    message["nonce"] = nonce_;
    message["expires_at"] = challengeExpiresAt_;
    sendJson(std::move(message));

    registrationTimer_.expires_after(kRegistrationTimeout);
    registrationTimer_.async_wait(boost::asio::bind_executor(
        strand_, [self = shared_from_this()](beast::error_code error) {
            if (!error && !self->registered_) {
                self->requestClose(websocket::close_code::policy_error, "registration timeout");
            }
        }));
}

void WebSocketSession::readMessage() {
    websocket_.async_read(
        buffer_, boost::asio::bind_executor(
                     strand_, [self = shared_from_this()](beast::error_code error, std::size_t) {
                         self->onRead(error);
                     }));
}

void WebSocketSession::onRead(beast::error_code error) {
    if (error == websocket::error::closed || error == boost::asio::error::operation_aborted) {
        return;
    }
    if (error) {
        std::cerr << "websocket read failed: " << error.message() << '\n';
        return;
    }

    if (!websocket_.got_text()) {
        buffer_.consume(buffer_.size());
        sendError("E_TEXT_REQUIRED", "signaling messages must be UTF-8 JSON text", true);
        return;
    }

    const std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    try {
        handleMessage(message);
    } catch (const std::exception& exception) {
        std::cerr << "websocket message handling failed: " << exception.what() << '\n';
        sendError("E_INTERNAL", "the server could not process this message", true);
    }

    if (!closing_) {
        readMessage();
    }
}

void WebSocketSession::handleMessage(const std::string& message) {
    boost::system::error_code error;
    boost::json::value value = boost::json::parse(message, error);
    if (error || !value.is_object()) {
        sendError("E_INVALID_JSON", "message must be a JSON object", true);
        return;
    }

    const auto& object = value.as_object();
    const std::string type = jsonString(object, "type");
    if (!registered_) {
        if (type != "register.response") {
            sendError("E_REGISTRATION_REQUIRED", "complete registration before other messages", true);
            return;
        }
        handleRegistration(object);
        return;
    }

    if (type == "register.response") {
        sendError("E_ALREADY_REGISTERED", "this connection is already registered", false);
    } else if (type == "presence.subscribe") {
        handlePresenceSubscription(object);
    } else if (isPairMessage(type)) {
        handleSignedEnvelope(object);
    } else {
        sendError("E_UNSUPPORTED_MESSAGE", "this signaling message is not implemented yet", false);
    }
}

void WebSocketSession::handleRegistration(const boost::json::object& object) {
    if (jsonString(object, "protocol") != kProtocol) {
        sendError("E_PROTOCOL_VERSION", "unsupported signaling protocol", true);
        return;
    }

    auto parsed = protocol::parseRegistrationResponse(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, true);
        return;
    }
    auto envelope = std::get<protocol::RegistrationEnvelope>(std::move(parsed));

    const std::int64_t now = unixSeconds();
    if (envelope.nonce != nonce_ || now > challengeExpiresAt_) {
        sendError("E_CHALLENGE_EXPIRED", "registration challenge is invalid or expired", true);
        return;
    }
    if (envelope.timestamp < now - kTimestampTolerance.count() ||
        envelope.timestamp > now + kTimestampTolerance.count()) {
        sendError("E_TIMESTAMP", "registration timestamp is outside the allowed window", true);
        return;
    }
    if (!protocol::verifyRegistrationEnvelope(envelope)) {
        sendError("E_SIGNATURE", "registration signature is invalid", true);
        return;
    }

    const auto result = devices_.registerDevice({
        .locatorId = envelope.deviceId,
        .publicKey = envelope.publicKey,
        .displayName = envelope.displayName,
        .platform = envelope.platform,
        .schemaVersion = static_cast<int>(envelope.schemaVersion),
        .registeredAt = now,
    });
    if (result == devices::RegistrationResult::locatorCollision) {
        sendError("E_ID_COLLISION", "device ID belongs to a different public key", true);
        return;
    }
    if (result == devices::RegistrationResult::identityConflict) {
        sendError("E_IDENTITY_CONFLICT", "public key is already bound to another device ID", true);
        return;
    }

    deviceId_ = envelope.deviceId;
    if (!hub_.connect(deviceId_, shared_from_this())) {
        deviceId_.clear();
        sendError("E_ALREADY_ONLINE", "this device already has an active connection", true);
        return;
    }
    connectedToHub_ = true;
    registered_ = true;
    publicKey_ = envelope.publicKey;
    registrationTimer_.cancel();

    boost::json::object response;
    response["type"] = "register.result";
    response["status"] = result == devices::RegistrationResult::created ? "created" : "refreshed";
    response["device_id"] = deviceId_;
    response["fingerprint"] = security::sha256Fingerprint(envelope.publicKey);
    sendJson(std::move(response));
}

void WebSocketSession::handlePresenceSubscription(const boost::json::object& object) {
    const auto* value = object.if_contains("device_ids");
    if (value == nullptr || !value->is_array() || value->as_array().size() > 100) {
        sendError("E_INVALID_SUBSCRIPTION", "device_ids must be an array of at most 100 IDs", false);
        return;
    }

    std::vector<std::string> deviceIds;
    std::unordered_set<std::string> unique;
    deviceIds.reserve(value->as_array().size());
    for (const auto& item : value->as_array()) {
        if (!item.is_string()) {
            sendError("E_INVALID_SUBSCRIPTION", "every subscribed device ID must be text", false);
            return;
        }
        const auto& text = item.as_string();
        std::string deviceId(text.data(), text.size());
        if (!isDeviceId(deviceId)) {
            sendError("E_INVALID_SUBSCRIPTION", "every subscribed device ID must contain 9 digits",
                      false);
            return;
        }
        if (unique.insert(deviceId).second) {
            deviceIds.push_back(std::move(deviceId));
        }
    }

    auto states = hub_.subscribe(deviceId_, this, deviceIds);
    if (!states) {
        sendError("E_CONNECTION_STATE", "the registered connection is no longer active", true);
        return;
    }

    boost::json::array devices;
    devices.reserve(states->size());
    for (const auto& state : *states) {
        boost::json::object item;
        item["device_id"] = state.deviceId;
        item["online"] = state.online;
        devices.push_back(std::move(item));
    }

    boost::json::object response;
    response["type"] = "presence.snapshot";
    response["devices"] = std::move(devices);
    sendJson(std::move(response));
}

void WebSocketSession::handleSignedEnvelope(const boost::json::object& object) {
    auto parsed = protocol::parseSignedEnvelope(object);
    if (const auto* protocolError = std::get_if<protocol::ProtocolError>(&parsed)) {
        sendError(protocolError->code, protocolError->message, false);
        return;
    }
    auto envelope = std::get<protocol::SignedEnvelope>(std::move(parsed));
    if (!isPairMessage(envelope.type) || !isDeviceId(envelope.targetId) ||
        envelope.targetId == deviceId_ || !isSessionId(envelope.sessionId)) {
        sendError("E_INVALID_ROUTE", "pairing route fields are invalid", false);
        return;
    }

    const std::int64_t now = unixSeconds();
    if (envelope.expiresAt < now || envelope.expiresAt > now + kTimestampTolerance.count()) {
        sendError("E_EXPIRED", "signed signaling message is expired or too far in the future", false);
        return;
    }
    if (!protocol::verifySignedEnvelope(envelope, deviceId_, publicKey_)) {
        sendError("E_SIGNATURE", "signaling signature is invalid", false);
        return;
    }

    std::string replayKey = envelope.type;
    replayKey.push_back('\0');
    replayKey += envelope.targetId;
    replayKey.push_back('\0');
    replayKey += envelope.sessionId;
    replayKey.push_back('\0');
    replayKey += std::to_string(envelope.sessionEpoch);
    const auto previous = replayWindows_.find(replayKey);
    if (previous != replayWindows_.end() && envelope.sequence <= previous->second) {
        sendError("E_REPLAY", "signaling sequence was already processed", false);
        return;
    }
    if (previous == replayWindows_.end() && replayWindows_.size() >= 256) {
        sendError("E_ROUTE_LIMIT", "too many active signaling routes", false);
        return;
    }

    if (envelope.type == "pair.request") {
        const auto steadyNow = std::chrono::steady_clock::now();
        const auto cutoff = steadyNow - std::chrono::minutes(1);
        while (!pairRequestTimes_.empty() && pairRequestTimes_.front() <= cutoff) {
            pairRequestTimes_.pop_front();
        }
        if (pairRequestTimes_.size() >= 10) {
            sendError("E_RATE_LIMIT", "too many pairing requests", false);
            return;
        }
        pairRequestTimes_.push_back(steadyNow);
    }

    replayWindows_[std::move(replayKey)] = envelope.sequence;
    const auto routeResult = hub_.route(deviceId_, this, envelope.targetId,
                                        protocol::signedEnvelopeToJson(envelope, deviceId_));
    if (routeResult == hub::RouteResult::senderMissing) {
        sendError("E_CONNECTION_STATE", "the registered connection is no longer active", true);
        return;
    }
    if (routeResult == hub::RouteResult::targetOffline) {
        sendError("E_TARGET_OFFLINE", "the target device is not online", false);
        return;
    }

    boost::json::object delivered;
    delivered["type"] = "signal.delivered";
    delivered["message_type"] = envelope.type;
    delivered["session_id"] = envelope.sessionId;
    delivered["sequence"] = envelope.sequence;
    sendJson(std::move(delivered));
}

void WebSocketSession::sendJson(boost::json::object message) {
    auto payload = std::make_shared<const std::string>(boost::json::serialize(message));
    outbound_.push_back(std::move(payload));
    if (outbound_.size() == 1) {
        writeNext();
    }
}

void WebSocketSession::sendError(std::string code, std::string message, bool closeAfter) {
    boost::json::object response;
    response["type"] = "error";
    response["code"] = std::move(code);
    response["message"] = std::move(message);
    sendJson(std::move(response));
    if (closeAfter) {
        requestClose(websocket::close_code::policy_error, "signaling policy violation");
    }
}

void WebSocketSession::writeNext() {
    websocket_.text(true);
    websocket_.async_write(
        boost::asio::buffer(*outbound_.front()),
        boost::asio::bind_executor(
            strand_, [self = shared_from_this()](beast::error_code error, std::size_t) {
                self->onWrite(error);
            }));
}

void WebSocketSession::onWrite(beast::error_code error) {
    if (error) {
        return;
    }
    outbound_.pop_front();
    if (!outbound_.empty()) {
        writeNext();
    } else if (closeWhenDrained_) {
        closeNow();
    }
}

void WebSocketSession::requestClose(websocket::close_code code, std::string reason) {
    if (closing_) {
        return;
    }
    closing_ = true;
    closeWhenDrained_ = true;
    closeReason_.code = code;
    closeReason_.reason = std::move(reason);
    registrationTimer_.cancel();
    if (outbound_.empty()) {
        closeNow();
    }
}

void WebSocketSession::closeNow() {
    websocket_.async_close(closeReason_, boost::asio::bind_executor(
                                             strand_, [self = shared_from_this()](beast::error_code) {
                                                 self->outbound_.clear();
                                             }));
}

}  // namespace easydesk::net
