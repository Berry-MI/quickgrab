#include "quickgrab/controller/AuthController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace quickgrab::controller {
namespace {

std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch == '+') {
            result.push_back(' ');
        } else if (ch == '%' && i + 2 < value.size()) {
            unsigned int code = 0;
            std::istringstream iss(value.substr(i + 1, 2));
            iss >> std::hex >> code;
            result.push_back(static_cast<char>(code));
            i += 2;
        } else {
            result.push_back(static_cast<char>(ch));
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string& body) {
    std::unordered_map<std::string, std::string> params;
    std::size_t start = 0;
    while (start < body.size()) {
        auto end = body.find('&', start);
        if (end == std::string::npos) {
            end = body.size();
        }
        auto token = body.substr(start, end - start);
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            auto key = urlDecode(token.substr(0, eq));
            auto value = urlDecode(token.substr(eq + 1));
            params.emplace(std::move(key), std::move(value));
        } else if (!token.empty()) {
            params.emplace(urlDecode(token), "");
        }
        start = end + 1;
    }
    return params;
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
                      const boost::json::value& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = boost::json::serialize(body);
    ctx.response.prepare_payload();
}

std::string buildSessionCookie(const service::AuthService::SessionInfo& session) {
    std::ostringstream oss;
    oss << service::AuthService::kSessionCookie << '=' << session.token << "; Path=/; HttpOnly; SameSite=Lax";
    if (session.rememberMe) {
        auto now = std::chrono::system_clock::now();
        auto ttl = std::chrono::duration_cast<std::chrono::seconds>(session.expiresAt - now);
        if (ttl.count() < 0) {
            ttl = std::chrono::seconds{0};
        }
        oss << "; Max-Age=" << ttl.count();
    }
    return oss.str();
}

std::string buildExpiredCookie() {
    std::ostringstream oss;
    oss << service::AuthService::kSessionCookie << "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax";
    return oss.str();
}

} // namespace

AuthController::AuthController(service::AuthService& authService)
    : authService_(authService) {}

void AuthController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/login", [this](auto& ctx) { handleLogin(ctx); });
    router.addRoute("POST", "/api/logout", [this](auto& ctx) { handleLogout(ctx); });
}

void AuthController::handleLogin(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto itUser = form.find("username");
    auto itPassword = form.find("password");
    bool rememberMe = false;
    if (auto itRemember = form.find("remember-me"); itRemember != form.end()) {
        const auto& value = itRemember->second;
        rememberMe = value == "true" || value == "1" || value == "on";
    }

    const std::string username = itUser != form.end() ? itUser->second : std::string{};
    const std::string password = itPassword != form.end() ? itPassword->second : std::string{};

    auto authResult = authService_.authenticate(username, password, rememberMe);
    if (!authResult.success || !authResult.session) {
        boost::json::object payload{{"status", "error"}, {"message", authResult.message.empty() ? "账号或密码错误" : authResult.message}};
        auto status = authResult.serverError ? boost::beast::http::status::internal_server_error
                                             : boost::beast::http::status::unauthorized;
        sendJsonResponse(ctx, status, payload);
        return;
    }

    ctx.response.set(boost::beast::http::field::set_cookie, buildSessionCookie(*authResult.session));

    boost::json::object payload{{"status", "success"},
                                {"message", "Login successful"},
                                {"username", authResult.session->buyer.username},
                                {"accessLevel", authResult.session->buyer.accessLevel},
                                {"email", authResult.session->buyer.email}};
    sendJsonResponse(ctx, boost::beast::http::status::ok, payload);
}

void AuthController::handleLogout(quickgrab::server::RequestContext& ctx) {
    std::string token;
    if (auto header = ctx.request.find(boost::beast::http::field::cookie); header != ctx.request.end()) {
        auto cookies = parseCookies(std::string(header->value()));
        if (auto it = cookies.find(std::string(service::AuthService::kSessionCookie)); it != cookies.end()) {
            token = it->second;
        }
    }

    authService_.logout(token);
    ctx.response.set(boost::beast::http::field::set_cookie, buildExpiredCookie());

    boost::json::object payload{{"status", "success"}, {"message", "Logout successful"}};
    sendJsonResponse(ctx, boost::beast::http::status::ok, payload);
}

} // namespace quickgrab::controller
