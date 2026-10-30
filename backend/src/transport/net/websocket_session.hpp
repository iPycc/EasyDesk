#pragma once

#include "application/hub/signaling_peer.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json/object.hpp>

#include <cstdint>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace easydesk::devices {
class DeviceRepository;
}

namespace easydesk::hub {
class SignalingHub;
}

namespace easydesk::protocol {
enum class SessionMessageType;
struct SessionEnvelope;
}

namespace easydesk::session {
class AssistCodeRegistry;
class SessionAttemptLimiter;
class SessionRegistry;
}

namespace easydesk::net {

class WebSocketSession final : public std::enable_shared_from_this<WebSocketSession>,
                               public hub::SignalingPeer {
public:
    using Request = boost::beast::http::request<boost::beast::http::string_body>;

    WebSocketSession(boost::beast::tcp_stream stream,
                     devices::DeviceRepository& devices,
                     hub::SignalingHub& hub,
                     session::AssistCodeRegistry& assistCodes,
                     session::SessionAttemptLimiter& attemptLimiter,
                     session::SessionRegistry& sessions);
    ~WebSocketSession() override;

    void accept(Request request);
    void deliver(boost::json::object message) override;

private:
    void onAccept(boost::beast::error_code error);
    void sendChallenge();
    void readMessage();
    void onRead(boost::beast::error_code error);
    void handleMessage(const std::string& message);
    void handleRegistration(const boost::json::object& object);
    void handlePresenceSubscription(const boost::json::object& object);
    void handleSignedEnvelope(const boost::json::object& object);
    void handleAssistRefresh(const boost::json::object& object);
    void handleAssistResolve(const boost::json::object& object);
    void handleSessionRequest(const boost::json::object& object);
    void handleSessionAccept(const boost::json::object& object);
    void handleSessionDecision(const boost::json::object& object,
                               protocol::SessionMessageType expectedType);
    void handleSessionOffer(const boost::json::object& object);
    void handleSessionAnswer(const boost::json::object& object);
    void handleSessionIceSignal(const boost::json::object& object,
                                protocol::SessionMessageType expectedType);
    void sendSessionDelivered(const protocol::SessionEnvelope& envelope);
    void sendAssistCode();

    void sendJson(boost::json::object message);
    void sendError(std::string code, std::string message, bool closeAfter);
    void writeNext();
    void onWrite(boost::beast::error_code error);
    void requestClose(boost::beast::websocket::close_code code, std::string reason);
    void closeNow();

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::beast::websocket::stream<boost::beast::tcp_stream> websocket_;
    boost::asio::steady_timer registrationTimer_;
    devices::DeviceRepository& devices_;
    hub::SignalingHub& hub_;
    session::AssistCodeRegistry& assistCodes_;
    session::SessionAttemptLimiter& attemptLimiter_;
    session::SessionRegistry& sessions_;
    boost::beast::flat_buffer buffer_;
    std::deque<std::shared_ptr<const std::string>> outbound_;
    std::string nonce_;
    std::string deviceId_;
    std::vector<std::uint8_t> publicKey_;
    std::unordered_map<std::string, std::uint64_t> replayWindows_;
    std::deque<std::chrono::steady_clock::time_point> pairRequestTimes_;
    std::size_t assistResolveAttempts_ = 0;
    std::int64_t challengeExpiresAt_ = 0;
    boost::beast::websocket::close_reason closeReason_;
    bool registered_ = false;
    bool connectedToHub_ = false;
    bool closing_ = false;
    bool closeWhenDrained_ = false;
};

}  // namespace easydesk::net
