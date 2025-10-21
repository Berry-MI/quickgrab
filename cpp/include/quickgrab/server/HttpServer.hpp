#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

namespace quickgrab::service {
class AuthService;
}

namespace quickgrab::server {

class Router;

class HttpServer : public std::enable_shared_from_this<HttpServer> {
public:
    HttpServer(boost::asio::io_context& io,
               std::shared_ptr<Router> router,
               service::AuthService& authService,
               std::string host,
               unsigned short port,
               std::string loginPage = "/login.html");

    void start();
    void stop();

private:
    void doAccept();

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<Router> router_;
    service::AuthService& authService_;
    std::string host_;
    unsigned short port_{};
    bool running_{};
    std::string loginPage_;
};

} // namespace quickgrab::server
