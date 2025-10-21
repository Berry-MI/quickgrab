#include "quickgrab/server/HttpServer.hpp"
#include "quickgrab/server/Router.hpp"
#include "quickgrab/server/RequestContext.hpp"
#include "quickgrab/service/AuthService.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace quickgrab::server {
namespace {

std::string trim(std::string value) {
    auto begin = value.begin();
    while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = value.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::unordered_map<std::string, std::string> parseCookies(const std::string& header) {
    std::unordered_map<std::string, std::string> cookies;
    std::size_t start = 0;
    while (start < header.size()) {
        auto end = header.find(';', start);
        if (end == std::string::npos) {
            end = header.size();
        }
        auto pair = header.substr(start, end - start);
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            auto key = trim(pair.substr(0, eq));
            auto value = trim(pair.substr(eq + 1));
            if (!key.empty()) {
                cookies.emplace(std::move(key), std::move(value));
            }
        }
        start = end + 1;
    }
    return cookies;
}

std::optional<std::string> findCookie(const RequestContext::HttpRequest& request, std::string_view name) {
    auto header = request.find(boost::beast::http::field::cookie);
    if (header == request.end()) {
        return std::nullopt;
    }
    auto cookies = parseCookies(std::string(header->value()));
    auto it = cookies.find(std::string(name));
    if (it == cookies.end()) {
        return std::nullopt;
    }
    return it->second;
}

void writeUnauthorizedResponse(RequestContext& ctx,
                               bool redirectToLogin,
                               const std::string& loginPage) {
    if (redirectToLogin) {
        ctx.response.result(boost::beast::http::status::found);
        ctx.response.set(boost::beast::http::field::location, loginPage);
        ctx.response.set(boost::beast::http::field::content_type, "text/html; charset=utf-8");
        ctx.response.body() =
            "<html><head><meta http-equiv=\"refresh\" content=\"0;url=" + loginPage +
            "\"></head><body>Redirecting...</body></html>";
    } else {
        ctx.response.result(boost::beast::http::status::unauthorized);
        ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
        ctx.response.body() = "{\"status\":\"error\",\"message\":\"未登录\"}";
    }
    ctx.response.prepare_payload();
}

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(boost::asio::ip::tcp::socket socket,
               std::shared_ptr<Router> router,
               service::AuthService& authService,
               std::string loginPage)
        : stream_(std::move(socket))
        , router_(std::move(router))
        , authService_(authService)
        , loginPage_(std::move(loginPage)) {}

    void start() { readRequest(); }

private:
    void readRequest() {
        request_ = {};
        boost::beast::http::async_read(stream_, buffer_, request_,
            boost::asio::bind_executor(stream_.get_executor(),
            [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (ec == boost::beast::http::error::end_of_stream) {
                    self->doClose();
                    return;
                }
                if (ec) {
                    self->doClose();
                    return;
                }
                self->dispatch();
            }));
    }

    void dispatch() {
        RequestContext ctx;
        ctx.startedAt = std::chrono::steady_clock::now();
        ctx.request = std::move(request_);
        ctx.response.version(ctx.request.version());
        ctx.response.keep_alive(ctx.request.keep_alive());

        std::unordered_map<std::string, std::string> params;
        auto match = router_->resolve(ctx.request.method_string(), ctx.request.target(), params);
        ctx.pathParameters = std::move(params);

        if (!match) {
            ctx.response.result(boost::beast::http::status::not_found);
            ctx.response.set(boost::beast::http::field::content_type, "application/json");
            ctx.response.body() = "{\\\"error\\\":\\\"not_found\\\"}";
            ctx.response.prepare_payload();
        } else {
            bool handled = false;
            if (match->requireAuth) {
                auto token = findCookie(ctx.request, service::AuthService::kSessionCookie);
                auto session = token ? authService_.touchSession(*token) : std::nullopt;
                if (!session) {
                    writeUnauthorizedResponse(ctx, match->redirectToLogin, loginPage_);
                    handled = true;
                }
            }

            if (!handled) {
                match->handler(ctx);
                if (ctx.response.body().empty() &&
                    ctx.response.result() == boost::beast::http::status::unknown) {
                    ctx.response.result(boost::beast::http::status::no_content);
                    ctx.response.prepare_payload();
                }
            }
        }

        auto response = std::make_shared<RequestContext::HttpResponse>(std::move(ctx.response));
        boost::beast::http::async_write(stream_, *response,
            boost::asio::bind_executor(stream_.get_executor(),
            [self = shared_from_this(), response](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    self->doClose();
                    return;
                }
                if (!response->keep_alive()) {
                    self->doClose();
                    return;
                }
                self->readRequest();
            }));
    }

    void doClose() {
        boost::system::error_code ec;
        stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        stream_.socket().close(ec);
    }

    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    RequestContext::HttpRequest request_;
    std::shared_ptr<Router> router_;
    service::AuthService& authService_;
    std::string loginPage_;
};

} // namespace

HttpServer::HttpServer(boost::asio::io_context& io,
                       std::shared_ptr<Router> router,
                       service::AuthService& authService,
                       std::string host,
                       unsigned short port,
                       std::string loginPage)
    : io_(io)
    , acceptor_(io)
    , router_(std::move(router))
    , authService_(authService)
    , host_(std::move(host))
    , port_(port)
    , loginPage_(std::move(loginPage)) {}

void HttpServer::start() {
    if (running_) {
        return;
    }
    running_ = true;

    boost::asio::ip::tcp::endpoint endpoint{
        boost::asio::ip::make_address(host_), port_};

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    doAccept();
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    boost::system::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);
}

void HttpServer::doAccept() {
    acceptor_.async_accept(
        boost::asio::make_strand(io_),
        [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!self->running_) {
                return;
            }

            if (!ec) {
                std::make_shared<HttpSession>(std::move(socket), self->router_, self->authService_, self->loginPage_)
                    ->start();
            }

            self->doAccept();
        });
}

} // namespace quickgrab::server
