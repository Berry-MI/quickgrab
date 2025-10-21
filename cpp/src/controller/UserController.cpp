#include "quickgrab/controller/UserController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace quickgrab::controller {
namespace {

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
            std::string key = pair.substr(0, eq);
            std::string value = pair.substr(eq + 1);
            auto trim = [](std::string str) {
                auto begin = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) { return std::isspace(ch); });
                auto endIt = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
                if (begin >= endIt) {
                    return std::string{};
                }
                return std::string(begin, endIt);
            };
            cookies.emplace(trim(std::move(key)), trim(std::move(value)));
        }
        start = end + 1;
    }
    return cookies;
}

void sendJsonResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const boost::json::value& payload) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = boost::json::serialize(payload);
    ctx.response.prepare_payload();
}

void sendUnauthorized(quickgrab::server::RequestContext& ctx) {
    boost::json::object payload{{"status", "error"}, {"message", "未登录"}};
    sendJsonResponse(ctx, boost::beast::http::status::unauthorized, payload);
}

} // namespace

UserController::UserController(service::AuthService& authService)
    : authService_(authService) {}

void UserController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute(
        "GET",
        "/api/user",
        [this](auto& ctx) { handleGetUser(ctx); },
        { .requireAuth = true });
}

void UserController::handleGetUser(quickgrab::server::RequestContext& ctx) {
    std::string token;
    if (auto header = ctx.request.find(boost::beast::http::field::cookie); header != ctx.request.end()) {
        auto cookies = parseCookies(std::string(header->value()));
        if (auto it = cookies.find(std::string(service::AuthService::kSessionCookie)); it != cookies.end()) {
            token = it->second;
        }
    }

    auto buyer = authService_.getBuyerByToken(token);
    if (!buyer) {
        sendUnauthorized(ctx);
        return;
    }

    boost::json::object payload{{"username", buyer->username},
                                {"accessLevel", buyer->accessLevel},
                                {"email", buyer->email}};
    sendJsonResponse(ctx, boost::beast::http::status::ok, payload);
}

} // namespace quickgrab::controller
