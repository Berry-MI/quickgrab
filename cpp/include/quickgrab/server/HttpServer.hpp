#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

namespace quickgrab::server {

class Router;

class HttpServer : public std::enable_shared_from_this<HttpServer> {
public:
    HttpServer(boost::asio::io_context& io,
               std::shared_ptr<Router> router,
               std::string host,
               unsigned short port);

    void start();
    void stop();

private:
    void doAccept();

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<Router> router_;
    std::string host_;
    unsigned short port_{};
    bool running_{};
};

} // namespace quickgrab::server
