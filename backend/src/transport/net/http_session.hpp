#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <memory>

namespace easydesk::devices {
class DeviceRepository;
}

namespace easydesk::hub {
class SignalingHub;
}

namespace easydesk::session {
class AssistCodeRegistry;
class SessionAttemptLimiter;
class SessionRegistry;
}

namespace easydesk::turn {
class TurnCredentialIssuer;
}

namespace easydesk::net {

class HttpSession final : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(boost::asio::ip::tcp::socket socket,
                devices::DeviceRepository& devices,
                hub::SignalingHub& hub,
                session::AssistCodeRegistry& assistCodes,
                session::SessionAttemptLimiter& attemptLimiter,
                session::SessionRegistry& sessions,
                const turn::TurnCredentialIssuer* turnCredentials);

    void run();

private:
    using Response = boost::beast::http::response<boost::beast::http::string_body>;

    void readRequest();
    void writeResponse(Response response);
    void close();

    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request_parser<boost::beast::http::string_body> parser_;
    std::shared_ptr<Response> response_;
    devices::DeviceRepository& devices_;
    hub::SignalingHub& hub_;
    session::AssistCodeRegistry& assistCodes_;
    session::SessionAttemptLimiter& attemptLimiter_;
    session::SessionRegistry& sessions_;
    const turn::TurnCredentialIssuer* turnCredentials_;
};

}  // namespace easydesk::net
