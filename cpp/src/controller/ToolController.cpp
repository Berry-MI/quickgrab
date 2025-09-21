#include "quickgrab/controller/ToolController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <random>
#include <stdexcept>
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

boost::json::object makeStatus(int code, std::string description) {
    boost::json::object status;
    status["code"] = code;
    status["description"] = std::move(description);
    return status;
}

boost::json::object wrapResponse(boost::json::object status, boost::json::object result) {
    boost::json::object response;
    response["status"] = std::move(status);
    response["result"] = std::move(result);
    return response;
}

std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string& body) {
    std::unordered_map<std::string, std::string> values;
    std::size_t start = 0;
    while (start < body.size()) {
        auto end = body.find('&', start);
        if (end == std::string::npos) {
            end = body.size();
        }
        auto token = body.substr(start, end - start);
        auto eq = token.find('=');
        std::string key;
        std::string value;
        if (eq == std::string::npos) {
            key = token;
        } else {
            key = token.substr(0, eq);
            value = token.substr(eq + 1);
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
        values.emplace(decode(key), decode(value));
        start = end + 1;
    }
    return values;
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

boost::json::object parseJsonObject(const std::string& payload) {
    if (payload.empty()) {
        return {};
    }
    try {
        auto value = boost::json::parse(payload);
        if (value.is_object()) {
            return value.as_object();
        }
    } catch (const std::exception&) {
    }
    return {};
}

boost::json::object ensureJsonObject(const boost::json::value& value) {
    if (value.is_object()) {
        return value.as_object();
    }
    if (value.is_string()) {
        return parseJsonObject(std::string(value.as_string().c_str()));
    }
    return {};
}

int calculateDelayIncrementWithJitter(int stockQuantity) {
    if (stockQuantity < 5) {
        return 0;
    }
    double baseDelay = std::log10(static_cast<double>(stockQuantity)) * 1.05;
    double jitterRange = std::min(baseDelay * 0.2, 0.3);
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    double randomJitter = dist(rng) * jitterRange;
    auto value = static_cast<long>(std::llround(baseDelay + randomJitter));
    value = std::clamp<long>(value, 0, 3);
    return static_cast<int>(value);
}

std::string percentDecode(const std::string& input) {
    std::string decoded;
    decoded.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (ch == '%') {
            if (i + 2 < input.size()) {
                auto hex = input.substr(i + 1, 2);
                decoded.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
                i += 2;
            }
        } else if (ch == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(ch);
        }
    }
    return decoded;
}

boost::json::object parseLinkPayload(const std::string& link) {
    auto paramPos = link.find("param=");
    if (paramPos == std::string::npos) {
        return {};
    }
    auto start = paramPos + 6;
    auto end = link.find('&', start);
    std::string encoded = link.substr(start, end == std::string::npos ? std::string::npos : end - start);
    return parseJsonObject(percentDecode(encoded));
}

bool looksLikeValidCookie(const std::string& cookies) {
    if (cookies.empty()) {
        return false;
    }
    auto eq = cookies.find('=');
    auto semi = cookies.find(';');
    return eq != std::string::npos && semi != std::string::npos && eq < semi;
}
} // namespace

void ToolController::registerRoutes(quickgrab::server::Router& router) {
    auto bindGetNote = [this](auto& ctx) { handleGetNote(ctx); };
    router.addRoute("POST", "/getNote", bindGetNote);
    router.addRoute("POST", "/api/getNote", bindGetNote);

    auto bindFetchItemInfo = [this](auto& ctx) { handleFetchItemInfo(ctx); };
    router.addRoute("POST", "/fetchItemInfo", bindFetchItemInfo);
    router.addRoute("POST", "/api/fetchItemInfo", bindFetchItemInfo);

    auto bindCheckCookies = [this](auto& ctx) { handleCheckCookies(ctx); };
    router.addRoute("GET", "/checkCookiesValidity", bindCheckCookies);
    router.addRoute("GET", "/api/checkCookiesValidity", bindCheckCookies);

    auto bindCheckLatency = [this](auto& ctx) { handleCheckLatency(ctx); };
    router.addRoute("POST", "/checkLatency", bindCheckLatency);
    router.addRoute("POST", "/api/checkLatency", bindCheckLatency);
}

void ToolController::handleGetNote(quickgrab::server::RequestContext& ctx) {
    try {
        auto json = boost::json::parse(ctx.request.body());
        if (!json.is_object()) {
            throw std::invalid_argument("payload must be JSON object");
        }
        const auto& payload = json.as_object();

        boost::json::object customInfo;
        if (auto it = payload.if_contains("orderTemplate")) {
            customInfo = ensureJsonObject(*it);
        } else if (auto it = payload.if_contains("orderParameters")) {
            auto parsed = ensureJsonObject(*it);
            if (auto info = parsed.if_contains("custom_info"); info && info->is_object()) {
                customInfo = info->as_object();
            }
        }

        boost::json::object result;
        result["custom_info"] = std::move(customInfo);

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    } catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(400, ex.what()), {});
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, response);
    }
}

void ToolController::handleFetchItemInfo(quickgrab::server::RequestContext& ctx) {
    try {
        auto form = parseFormUrlEncoded(ctx.request.body());
        auto linkIt = form.find("link");
        auto cookieIt = form.find("cookies");
        if (linkIt == form.end() || linkIt->second.empty()) {
            throw std::invalid_argument("缺少链接参数");
        }
        if (cookieIt == form.end() || cookieIt->second.empty()) {
            throw std::invalid_argument("缺少Cookies参数");
        }

        auto param = parseLinkPayload(linkIt->second);
        int categoryCount = 0;
        int stockQuantity = 0;
        bool hasExpireDate = false;
        if (auto listIt = param.if_contains("item_list"); listIt && listIt->is_array()) {
            const auto& list = listIt->as_array();
            categoryCount = static_cast<int>(list.size());
            if (!list.empty()) {
                const auto& item = list.front();
                if (item.is_object()) {
                    const auto& obj = item.as_object();
                    if (auto stockIt = obj.if_contains("stock")) {
                        if (stockIt->is_int64()) {
                            stockQuantity = static_cast<int>(stockIt->as_int64());
                        }
                    }
                    if (auto convey = obj.if_contains("item_convey_info"); convey && convey->is_object()) {
                        if (auto valid = convey->as_object().if_contains("valid_date_info")) {
                            hasExpireDate = valid->is_object();
                        }
                    }
                }
            }
        }

        int delayIncrement = calculateDelayIncrementWithJitter(stockQuantity);

        boost::json::object result;
        result["delayIncrement"] = delayIncrement;
        result["hasExpireDate"] = hasExpireDate;
        result["isFutureSold"] = false;
        result["categoryCount"] = categoryCount;

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    } catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(400, ex.what()), {});
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, response);
    }
}

void ToolController::handleCheckCookies(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto it = params.find("cookies");
    bool valid = it != params.end() && looksLikeValidCookie(it->second);

    boost::json::object body;
    body["message"] = valid ? "Cookies有效" : "Cookies无效";
    sendJsonResponse(ctx, boost::beast::http::status::ok, std::move(body));
}

void ToolController::handleCheckLatency(quickgrab::server::RequestContext& ctx) {
    long latency = -1;
    try {
        auto json = boost::json::parse(ctx.request.body());
        if (json.is_object()) {
            const auto& obj = json.as_object();
            if (auto ext = obj.if_contains("extension")) {
                auto extension = ensureJsonObject(*ext);
                if (auto value = extension.if_contains("networkDelay")) {
                    if (value->is_int64()) {
                        latency = static_cast<long>(value->as_int64());
                    } else if (value->is_double()) {
                        latency = static_cast<long>(value->as_double());
                    }
                }
            }
            if (latency < 0) {
                if (auto delay = obj.if_contains("delay"); delay && delay->is_int64()) {
                    latency = static_cast<long>(delay->as_int64());
                }
            }
        }
    } catch (const std::exception&) {
        latency = -1;
    }

    ctx.response.result(boost::beast::http::status::ok);
    ctx.response.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
    if (latency >= 0) {
        ctx.response.body() = std::to_string(latency);
    } else {
        ctx.response.body().clear();
    }
    ctx.response.prepare_payload();
}

} // namespace quickgrab::controller
