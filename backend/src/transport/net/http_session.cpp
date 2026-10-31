#include "transport/net/http_session.hpp"

#include "transport/net/websocket_session.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/beast/websocket.hpp>

#include <chrono>
#include <string_view>
#include <utility>

namespace easydesk::net {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;

http::response<http::string_body> makeResponse(const http::request<http::string_body>& request) {
    http::response<http::string_body> response;
    response.version(request.version());
    response.keep_alive(false);
    response.set(http::field::server, "EasyDesk-Signal/0.1");
    response.set(http::field::content_type, "application/json; charset=utf-8");

    const std::string_view target(request.target().data(), request.target().size());
    if (request.method() == http::verb::get && (target == "/healthz" || target == "/readyz")) {
        response.result(http::status::ok);
        response.body() =
            R"({"status":"ok","service":"easydesk-signal","protocol":"easydesk-signaling/1"})";
    } else {
        response.result(http::status::not_found);
        response.body() = R"({"error":"not_found"})";
    }

    response.prepare_payload();
    return response;
}

}  // namespace

HttpSession::HttpSession(boost::asio::ip::tcp::socket socket,
                         devices::DeviceRepository& devices,
                         hub::SignalingHub& hub,
                         session::AssistCodeRegistry& assistCodes,
                         session::SessionAttemptLimiter& attemptLimiter,
                         session::SessionRegistry& sessions,
                         const turn::TurnCredentialIssuer* turnCredentials)
    : stream_(std::move(socket)),
      devices_(devices),
      hub_(hub),
      assistCodes_(assistCodes),
      attemptLimiter_(attemptLimiter),
      sessions_(sessions),
      turnCredentials_(turnCredentials) {
    parser_.body_limit(16 * 1024);
}

void HttpSession::run() {
    boost::asio::dispatch(stream_.get_executor(),
                          [self = shared_from_this()] { self->readRequest(); });
}

void HttpSession::readRequest() {
    stream_.expires_after(std::chrono::seconds(15));
    http::async_read(stream_, buffer_, parser_,
                     [self = shared_from_this()](beast::error_code error, std::size_t) {
                         if (error) {
                             self->close();
                             return;
                         }
                         auto request = self->parser_.release();
                         const std::string_view target(request.target().data(), request.target().size());
                         if (boost::beast::websocket::is_upgrade(request) && target == "/ws") {
                             self->stream_.expires_never();
                             std::make_shared<WebSocketSession>(std::move(self->stream_), self->devices_,
                                                                self->hub_, self->assistCodes_,
                                                                self->attemptLimiter_, self->sessions_,
                                                                self->turnCredentials_)
                                 ->accept(std::move(request));
                             return;
                         }
                         self->writeResponse(makeResponse(request));
                     });
}

void HttpSession::writeResponse(Response response) {
    response_ = std::make_shared<Response>(std::move(response));
    http::async_write(stream_, *response_,
                      [self = shared_from_this()](beast::error_code, std::size_t) {
                          self->close();
                      });
}

void HttpSession::close() {
    boost::system::error_code ignored;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    stream_.socket().close(ignored);
}

}  // namespace easydesk::net
