#include "transport/net/http_listener.hpp"

#include "transport/net/http_session.hpp"

#include <boost/asio/io_context.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace easydesk::net {

HttpListener::HttpListener(boost::asio::io_context& context,
                           const boost::asio::ip::tcp::endpoint& endpoint,
                           devices::DeviceRepository& devices,
                           hub::SignalingHub& hub,
                           session::AssistCodeRegistry& assistCodes,
                           session::SessionAttemptLimiter& attemptLimiter,
                           session::SessionRegistry& sessions,
                           const turn::TurnCredentialIssuer* turnCredentials)
    : acceptor_(context),
      devices_(devices),
      hub_(hub),
      assistCodes_(assistCodes),
      attemptLimiter_(attemptLimiter),
      sessions_(sessions),
      turnCredentials_(turnCredentials) {
    boost::system::error_code error;

    acceptor_.open(endpoint.protocol(), error);
    if (error) {
        throw std::runtime_error("open listener: " + error.message());
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), error);
    if (error) {
        throw std::runtime_error("configure listener: " + error.message());
    }

    acceptor_.bind(endpoint, error);
    if (error) {
        throw std::runtime_error("bind listener: " + error.message());
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, error);
    if (error) {
        throw std::runtime_error("listen: " + error.message());
    }
}

void HttpListener::run() {
    accept();
}

void HttpListener::accept() {
    acceptor_.async_accept([self = shared_from_this()](boost::system::error_code error,
                                                       boost::asio::ip::tcp::socket socket) {
        if (!error) {
            std::make_shared<HttpSession>(std::move(socket), self->devices_, self->hub_,
                                          self->assistCodes_, self->attemptLimiter_,
                                          self->sessions_, self->turnCredentials_)
                ->run();
        } else if (error != boost::asio::error::operation_aborted) {
            std::cerr << "accept failed: " << error.message() << '\n';
        }

        if (self->acceptor_.is_open()) {
            self->accept();
        }
    });
}

}  // namespace easydesk::net
