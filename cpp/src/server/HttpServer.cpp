#include "quickgrab/server/HttpServer.hpp"
#include "quickgrab/server/Router.hpp"
#include "quickgrab/server/RequestContext.hpp"
#include "quickgrab/util/JsonResponse.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace quickgrab::server {
namespace {

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<Router> router)
        : stream_(std::move(socket)), router_(std::move(router)) {}

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
        auto handler = router_->resolve(ctx.request.method_string(), ctx.request.target(), params);
        ctx.pathParameters = std::move(params);

        if (!handler) {
            ctx.response.result(boost::beast::http::status::not_found);
            ctx.response.set(boost::beast::http::field::content_type, "application/json");
            ctx.response.body() = "{\\\"error\\\":\\\"not_found\\\"}";
            ctx.response.prepare_payload();
        } else {
            handler(ctx);
            if (ctx.response.body().empty() && ctx.response.result() == boost::beast::http::status::unknown) {
                ctx.response.result(boost::beast::http::status::no_content);
                ctx.response.prepare_payload();
            }
        }

        wrapJsonEnvelope(ctx);

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

    void wrapJsonEnvelope(RequestContext& ctx) {
        auto& response = ctx.response;
        if (response.body().empty()) {
            return;
        }

        auto contentTypeIt = response.find(boost::beast::http::field::content_type);
        if (contentTypeIt == response.end()) {
            return;
        }

        std::string contentType = std::string(contentTypeIt->value());
        auto toLower = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        };

        if (contentType.find("application/json") == std::string::npos) {
            std::string lowered = toLower(contentType);
            if (lowered.find("application/json") == std::string::npos) {
                return;
            }
        }

        std::string target(ctx.request.target());
        if (target == "/api/login" || target == "/api/loginbyvcode") {
            return;
        }

        if (auto header = response.find("X-Api-Envelope"); header != response.end()) {
            std::string flag = toLower(std::string(header->value()));
            response.erase(header);
            if (flag == "skip") {
                return;
            }
        }

        bool success = response.result_int() >= 200 && response.result_int() < 400;

        boost::json::value parsed;
        try {
            parsed = boost::json::parse(response.body());
        } catch (const std::exception&) {
            parsed = boost::json::value(boost::json::string(response.body()));
        }

        std::string message;
        if (!success && parsed.is_object()) {
            const auto& object = parsed.as_object();
            if (auto it = object.if_contains("message"); it && it->is_string()) {
                message = std::string(it->as_string().c_str());
            } else if (auto it = object.if_contains("error"); it && it->is_string()) {
                message = std::string(it->as_string().c_str());
            }
        }

        boost::json::object envelope = success
            ? quickgrab::util::makeSuccessResponse(parsed, target)
            : quickgrab::util::makeErrorDetails(message, parsed, target);

        response.body() = boost::json::serialize(envelope);
        response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
        response.prepare_payload();
    }

    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    RequestContext::HttpRequest request_;
    std::shared_ptr<Router> router_;
};

} // namespace

HttpServer::HttpServer(boost::asio::io_context& io,
                       std::shared_ptr<Router> router,
                       std::string host,
                       unsigned short port)
    : io_(io)
    , acceptor_(io)
    , router_(std::move(router))
    , host_(std::move(host))
    , port_(port) {}

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
                std::make_shared<HttpSession>(std::move(socket), self->router_)->start();
            }

            self->doAccept();
        });
}

} // namespace quickgrab::server
