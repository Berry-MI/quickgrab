#include "quickgrab/controller/AuthController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

std::optional<std::string> findQueryParameter(boost::beast::string_view query, std::string_view key) {
    while (!query.empty()) {
        auto ampPos = query.find('&');
        auto token = ampPos == boost::beast::string_view::npos ? query : query.substr(0, ampPos);
        if (ampPos == boost::beast::string_view::npos) {
            query = {};
        } else {
            query.remove_prefix(ampPos + 1);
        }

        auto eqPos = token.find('=');
        if (eqPos == boost::beast::string_view::npos) {
            continue;
        }

        auto name = token.substr(0, eqPos);
        if (name.size() == key.size() &&
            std::equal(name.begin(), name.end(), key.begin(), key.end())) {
            auto value = token.substr(eqPos + 1);
            return urlDecode(std::string(value));
        }
    }

    return std::nullopt;
}

std::optional<std::string> extractOriginalUri(const quickgrab::server::RequestContext& ctx) {
    static constexpr std::array<std::string_view, 5> kHeaderKeys{
        "X-Original-URI", "X-Original-URL", "X-Forwarded-Uri", "X-Forwarded-URL", "X-Rewrite-URL",
    };

    for (auto header : kHeaderKeys) {
        auto it = ctx.request.base().find(header);
        if (it != ctx.request.base().end() && !it->value().empty()) {
            return std::string(it->value());
        }
    }

    auto target = ctx.request.target();
    auto queryPos = target.find('?');
    if (queryPos == boost::beast::string_view::npos) {
        return std::nullopt;
    }

    auto query = target.substr(queryPos + 1);
    static constexpr std::array<std::string_view, 4> kQueryKeys{"uri", "url", "target", "path"};
    for (auto key : kQueryKeys) {
        if (auto value = findQueryParameter(query, key)) {
            return value;
        }
    }

    return std::nullopt;
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

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        unsigned char left = static_cast<unsigned char>(lhs[i]);
        unsigned char right = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }
    return true;
}

bool isPublicResourcePath(std::string_view path) {
    if (path.empty()) {
        return false;
    }

    if (path == "/login" || path == "/login/" || path == "/login.html") {
        return true;
    }

    if (path == "/favicon.ico" || path == "/robots.txt") {
        return true;
    }

    auto filenamePos = path.find_last_of('/');
    std::string_view filename = filenamePos == std::string_view::npos ? path : path.substr(filenamePos + 1);
    if (filename.empty()) {
        return false;
    }

    auto dotPos = filename.find_last_of('.');
    if (dotPos == std::string_view::npos) {
        return false;
    }

    std::string_view ext = filename.substr(dotPos + 1);
    static constexpr std::array<std::string_view, 16> kAllowedExtensions{
        "css", "js",  "png",  "jpg",  "jpeg", "gif",  "svg",  "webp",
        "bmp", "ico", "woff", "woff2", "ttf",  "otf", "eot",  "map",
    };

    for (auto allowed : kAllowedExtensions) {
        if (equalsIgnoreCase(ext, allowed)) {
            return true;
        }
    }

    return false;
}

bool shouldBypassProbe(const quickgrab::server::RequestContext& ctx) {
    auto originalUri = extractOriginalUri(ctx);
    if (!originalUri) {
        return false;
    }

    std::string_view view(*originalUri);
    auto queryPos = view.find('?');
    if (queryPos != std::string_view::npos) {
        view = view.substr(0, queryPos);
    }

    if (view.empty()) {
        return false;
    }

    return isPublicResourcePath(view);
}

void sendJsonResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const boost::json::value& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = boost::json::serialize(body);
    ctx.response.prepare_payload();
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
    router.addRoute("GET", "/api/session", [this](auto& ctx) {
        handleSessionStatus(ctx, SessionResponseMode::full);
    });
    router.addRoute("GET", "/internal/auth/check", [this](auto& ctx) {
        handleSessionStatus(ctx, SessionResponseMode::probe);
    });
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

    ctx.response.set(boost::beast::http::field::set_cookie, authService_.buildSessionCookie(*authResult.session));

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

void AuthController::handleSessionStatus(quickgrab::server::RequestContext& ctx,
                                         SessionResponseMode mode) {
    std::string token;
    if (auto header = ctx.request.find(boost::beast::http::field::cookie); header != ctx.request.end()) {
        auto cookies = parseCookies(std::string(header->value()));
        if (auto it = cookies.find(std::string(service::AuthService::kSessionCookie)); it != cookies.end()) {
            token = it->second;
        }
    }

    auto session = authService_.touchSession(token);
    if (!session) {
        if (mode == SessionResponseMode::probe && shouldBypassProbe(ctx)) {
            ctx.response.result(boost::beast::http::status::no_content);
            ctx.response.prepare_payload();
            return;
        }

        ctx.response.set(boost::beast::http::field::set_cookie, buildExpiredCookie());
        if (mode == SessionResponseMode::full) {
            boost::json::object payload{{"status", "error"}, {"message", "未登录"}};
            sendJsonResponse(ctx, boost::beast::http::status::unauthorized, payload);
        } else {
            ctx.response.result(boost::beast::http::status::unauthorized);
            ctx.response.prepare_payload();
        }
        return;
    }

    if (mode == SessionResponseMode::full) {
        boost::json::object payload{{"status", "success"},
                                    {"username", session->buyer.username},
                                    {"accessLevel", session->buyer.accessLevel},
                                    {"email", session->buyer.email}};
        sendJsonResponse(ctx, boost::beast::http::status::ok, payload);
    } else {
        ctx.response.result(boost::beast::http::status::no_content);
        ctx.response.prepare_payload();
    }
}

} // namespace quickgrab::controller
