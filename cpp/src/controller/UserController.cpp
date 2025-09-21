#include "quickgrab/controller/UserController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <string>
#include <unordered_map>

namespace quickgrab::controller {
namespace {
void sendJsonResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const boost::json::value& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = boost::json::serialize(body);
    ctx.response.prepare_payload();
}

std::unordered_map<std::string, std::string> parseQueryParameters(const boost::beast::string_view& target) {
    std::unordered_map<std::string, std::string> params;
    auto pos = target.find('?');
    if (pos == boost::beast::string_view::npos) {
        return params;
    }
    auto query = target.substr(pos + 1);
    std::size_t start = 0;
    while (start < query.size()) {
        auto end = query.find('&', start);
        if (end == boost::beast::string_view::npos) {
            end = query.size();
        }
        auto token = query.substr(start, end - start);
        auto eq = token.find('=');
        std::string key;
        std::string value;
        if (eq == boost::beast::string_view::npos) {
            key.assign(token.data(), token.size());
        } else {
            key.assign(token.substr(0, eq).data(), eq);
            value.assign(token.substr(eq + 1).data(), token.size() - eq - 1);
        }
        auto decode = [](std::string input) {
            std::string decoded;
            decoded.reserve(input.size());
            for (std::size_t i = 0; i < input.size(); ++i) {
                char ch = input[i];
                if (ch == '+') {
                    decoded.push_back(' ');
                } else if (ch == '%' && i + 2 < input.size()) {
                    auto hex = input.substr(i + 1, 2);
                    decoded.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
                    i += 2;
                } else {
                    decoded.push_back(ch);
                }
            }
            return decoded;
        };
        params.emplace(decode(std::move(key)), decode(std::move(value)));
        start = end + 1;
    }
    return params;
}

} // namespace

void UserController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/user", [this](auto& ctx) { handleGetUser(ctx); });
}

void UserController::handleGetUser(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());

    std::string username;
    if (auto it = params.find("username"); it != params.end() && !it->second.empty()) {
        username = it->second;
    } else if (auto header = ctx.request.find("X-User"); header != ctx.request.end()) {
        username = std::string(header->value());
    } else if (auto auth = ctx.request.find(boost::beast::http::field::authorization);
               auth != ctx.request.end() && !auth->value().empty()) {
        username = std::string(auth->value());
    }

    if (username.empty()) {
        username = "guest";
    }

    boost::json::object userInfo{{"username", username}, {"accessLevel", 1}, {"email", username + "@example.com"}};
    sendJsonResponse(ctx, boost::beast::http::status::ok, std::move(userInfo));
}

} // namespace quickgrab::controller
