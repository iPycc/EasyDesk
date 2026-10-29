#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace boost::asio {
class io_context;
}

namespace easydesk::net {

class HttpListener final : public std::enable_shared_from_this<HttpListener> {
public:
    HttpListener(boost::asio::io_context& context, const boost::asio::ip::tcp::endpoint& endpoint);

    void run();

private:
    void accept();

    boost::asio::ip::tcp::acceptor acceptor_;
};

}  // namespace easydesk::net
