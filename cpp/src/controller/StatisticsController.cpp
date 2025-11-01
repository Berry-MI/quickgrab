#include "quickgrab/controller/StatisticsController.hpp"
#include "quickgrab/util/JsonUtil.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>

namespace quickgrab::controller {
namespace {
std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '+') {
            result.push_back(' ');
        } else if (c == '%' && i + 2 < value.size()) {
            auto hex = value.substr(i + 1, 2);
            char decoded = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result.push_back(decoded);
            i += 2;
        } else {
            result.push_back(c);
        }
    }
    return result;
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
        if (eq != boost::beast::string_view::npos) {
            std::string key = urlDecode(std::string(token.substr(0, eq)));
            std::string value = urlDecode(std::string(token.substr(eq + 1)));
            params.emplace(std::move(key), std::move(value));
        } else {
            params.emplace(urlDecode(std::string(token)), "");
        }
        start = end + 1;
    }
    return params;
}

std::optional<int> parseOptionalInt(const std::unordered_map<std::string, std::string>& params,
                                    const std::string& key) {
    auto it = params.find(key);
    if (it == params.end() || it->second.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoi(it->second);
    } catch (...) {
        return std::nullopt;
    }
}

void sendJson(quickgrab::server::RequestContext& ctx, const boost::json::value& value) {
    ctx.response.result(boost::beast::http::status::ok);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = quickgrab::util::stringifyJson(value);
    ctx.response.prepare_payload();
}

} // namespace

StatisticsController::StatisticsController(service::StatisticsService& statisticsService)
    : statisticsService_(statisticsService) {}

void StatisticsController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/get/statistics", [this](auto& ctx) { handleStatistics(ctx); });
    router.addRoute("GET", "/api/get/statistics/daily", [this](auto& ctx) { handleDailyStats(ctx); });
    router.addRoute("GET", "/api/get/statistics/hourly", [this](auto& ctx) { handleHourlyStats(ctx); });
    router.addRoute("GET", "/api/get/statistics/buyers", [this](auto& ctx) { handleBuyers(ctx); });
}

static std::optional<std::string> normalizeIsoToMysql(std::optional<std::string> iso) {
    if (!iso || iso->empty()) return std::nullopt;
    std::string s = *iso;

    // 末尾 Z
    if (!s.empty() && (s.back() == 'Z' || s.back() == 'z')) s.pop_back();

    // 毫秒
    auto dot = s.find('.');
    if (dot != std::string::npos) s.erase(dot);

    // T -> ' '
    for (auto& c : s) if (c == 'T' || c == 't') c = ' ';

    // 现在是 "YYYY-MM-DD HH:MM:SS"
    return s;
}

void StatisticsController::handleStatistics(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto buyerId = parseOptionalInt(params, "buyerId");

    std::optional<std::string> startIso, endIso;
    if (auto it = params.find("startTime"); it != params.end() && !it->second.empty()) startIso = it->second;
    if (auto it = params.find("endTime");   it != params.end() && !it->second.empty()) endIso = it->second;

    // 规范化为 "YYYY-MM-DD HH:MM:SS"
    auto startTime = normalizeIsoToMysql(startIso);
    auto endTime = normalizeIsoToMysql(endIso);

    auto stats = statisticsService_.getStatistics(buyerId, startTime, endTime);
    boost::json::object response;
    boost::json::array typeStats;

    for (const auto& entry : stats) {
        boost::json::object item;
        item["type"] = entry.type;
        item["successCount"] = entry.successCount;
        item["failureCount"] = entry.failureCount;
        item["exceptionCount"] = entry.exceptionCount;
        item["totalCount"] = entry.totalCount;
        item["successEarnings"] = entry.successEarnings;
        item["failureEarnings"] = entry.failureEarnings;
        item["exceptionEarnings"] = entry.exceptionEarnings;
        item["totalEarnings"] = entry.totalEarnings;

        if (entry.type == "total" || entry.type.empty()) {
            response = item;
        } else {
            typeStats.emplace_back(std::move(item));
        }
    }

    if (!response.if_contains("successCount")) {
        response["type"] = "total";
        response["successCount"] = 0;
        response["failureCount"] = 0;
        response["exceptionCount"] = 0;
        response["totalCount"] = 0;
        response["successEarnings"] = 0.0;
        response["failureEarnings"] = 0.0;
        response["exceptionEarnings"] = 0.0;
        response["totalEarnings"] = 0.0;
    }
    typeStats.emplace_back(response); 
    response["typeStats"] = typeStats;
    sendJson(ctx, response);
}

void StatisticsController::handleDailyStats(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto buyerId = parseOptionalInt(params, "buyerId");
    auto status = parseOptionalInt(params, "status");

    auto stats = statisticsService_.getDailyStats(buyerId, status);
    boost::json::array payload;
    payload.reserve(stats.size());
    for (const auto& entry : stats) {
        boost::json::object item;
        item["date"] = entry.date;
        item["total"] = entry.total;
        item["earnings"] = entry.earnings;
        payload.emplace_back(std::move(item));
    }
    sendJson(ctx, payload);
}

void StatisticsController::handleHourlyStats(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto buyerId = parseOptionalInt(params, "buyerId");
    auto status = parseOptionalInt(params, "status");

    auto stats = statisticsService_.getHourlyStats(buyerId, status);
    boost::json::array payload;
    payload.reserve(stats.size());
    for (const auto& entry : stats) {
        boost::json::object item;
        item["hour"] = entry.hour;
        item["total"] = entry.total;
        item["earnings"] = entry.earnings;
        payload.emplace_back(std::move(item));
    }
    sendJson(ctx, payload);
}

void StatisticsController::handleBuyers(quickgrab::server::RequestContext& ctx) {
    auto buyers = statisticsService_.getAllBuyers();
    boost::json::array payload;
    payload.reserve(buyers.size());
    for (const auto& buyer : buyers) {
        boost::json::object item;
        item["id"] = buyer.id;
        item["username"] = buyer.username;
        payload.emplace_back(std::move(item));
    }
    sendJson(ctx, payload);
}

} // namespace quickgrab::controller
