#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace boost::asio {
class io_context;
}

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

namespace easydesk::net {

class HttpListener final : public std::enable_shared_from_this<HttpListener> {
public:
    HttpListener(boost::asio::io_context& context,
                 const boost::asio::ip::tcp::endpoint& endpoint,
                 devices::DeviceRepository& devices,
                 hub::SignalingHub& hub,
                 session::AssistCodeRegistry& assistCodes,
                 session::SessionAttemptLimiter& attemptLimiter,
                 session::SessionRegistry& sessions);

    void run();

private:
    void accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    devices::DeviceRepository& devices_;
    hub::SignalingHub& hub_;
    session::AssistCodeRegistry& assistCodes_;
    session::SessionAttemptLimiter& attemptLimiter_;
    session::SessionRegistry& sessions_;
};

}  // namespace easydesk::net
