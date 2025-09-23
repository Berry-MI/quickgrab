#include "quickgrab/controller/QueryController.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
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

int parseIntOrDefault(const std::unordered_map<std::string, std::string>& params,
                      const std::string& key,
                      int fallback) {
    if (auto value = parseOptionalInt(params, key)) {
        return *value;
    }
    return fallback;
}

std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) {
        return "";
    }
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

boost::json::object requestToJson(const model::Request& request) {
    boost::json::object obj;
    obj["id"] = request.id;
    obj["deviceId"] = request.deviceId;
    obj["buyerId"] = request.buyerId;
    obj["threadId"] = request.threadId;
    obj["link"] = request.link;
    obj["cookies"] = request.cookies;
    obj["orderInfo"] = request.orderInfo;
    obj["userInfo"] = request.userInfo;
    obj["orderTemplate"] = request.orderTemplate;
    obj["message"] = request.message;
    obj["idNumber"] = request.idNumber;
    obj["keyword"] = request.keyword;
    obj["startTime"] = formatTimestamp(request.startTime);
    obj["endTime"] = formatTimestamp(request.endTime);
    obj["quantity"] = request.quantity;
    obj["delay"] = request.delay;
    obj["frequency"] = request.frequency;
    obj["type"] = request.type;
    obj["status"] = request.status;
    if (!request.orderParametersRaw.empty()) {
        obj["orderParameters"] = request.orderParametersRaw;
    } else {
        obj["orderParameters"] = request.orderParameters;
    }
    if (!request.orderParametersRaw.empty()) {
        obj["orderParametersRaw"] = request.orderParametersRaw;
    }
    obj["actualEarnings"] = request.actualEarnings;
    obj["estimatedEarnings"] = request.estimatedEarnings;
    obj["extension"] = request.extension;
    return obj;
}

boost::json::object resultToJson(const model::Result& result) {
    boost::json::object obj;
    obj["id"] = result.id;
    obj["requestId"] = result.requestId;
    obj["deviceId"] = result.deviceId;
    obj["buyerId"] = result.buyerId;
    obj["threadId"] = result.threadId;
    obj["link"] = result.link;
    obj["cookies"] = result.cookies;
    obj["orderInfo"] = result.orderInfo;
    obj["userInfo"] = result.userInfo;
    obj["orderTemplate"] = result.orderTemplate;
    obj["message"] = result.message;
    obj["idNumber"] = result.idNumber;
    obj["keyword"] = result.keyword;
    obj["startTime"] = formatTimestamp(result.startTime);
    obj["endTime"] = formatTimestamp(result.endTime);
    obj["quantity"] = result.quantity;
    obj["delay"] = result.delay;
    obj["frequency"] = result.frequency;
    obj["type"] = result.type;
    obj["status"] = result.status;
    obj["responseMessage"] = result.responseMessage;
    obj["actualEarnings"] = result.actualEarnings;
    obj["estimatedEarnings"] = result.estimatedEarnings;
    obj["extension"] = result.extension;
    obj["payload"] = result.payload;
    obj["createdAt"] = formatTimestamp(result.createdAt);
    return obj;
}

boost::json::object buyerToJson(const model::Buyer& buyer) {
    boost::json::object obj;
    obj["id"] = buyer.id;
    obj["username"] = buyer.username;
    return obj;
}

void sendJsonResponse(quickgrab::server::RequestContext& ctx, const boost::json::value& payload) {
    ctx.response.result(boost::beast::http::status::ok);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = quickgrab::util::stringifyJson(payload);
    ctx.response.prepare_payload();
}

void sendNotFound(quickgrab::server::RequestContext& ctx) {
    ctx.response.result(boost::beast::http::status::not_found);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = "{\"error\":\"not_found\"}";
    ctx.response.prepare_payload();
}

void sendServerError(quickgrab::server::RequestContext& ctx, std::string_view message) {
    ctx.response.result(boost::beast::http::status::internal_server_error);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    if (!message.empty()) {
        boost::json::object obj;
        obj["error"] = "internal_error";
        obj["message"] = message;
        ctx.response.body() = quickgrab::util::stringifyJson(obj);
    } else {
        ctx.response.body() = "{\"error\":\"internal_error\"}";
    }
    ctx.response.prepare_payload();
}

} // namespace

QueryController::QueryController(service::QueryService& queryService)
    : queryService_(queryService) {}

void QueryController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/grab/pending", [this](auto& ctx) { handlePending(ctx); });

    auto bindGetRequests = [this](auto& ctx) { handleGetRequests(ctx); };
    router.addRoute("GET", "/getRequests", bindGetRequests);
    router.addRoute("GET", "/api/getRequests", bindGetRequests);

    auto bindGetResults = [this](auto& ctx) { handleGetResults(ctx); };
    router.addRoute("GET", "/getResults", bindGetResults);
    router.addRoute("GET", "/api/getResults", bindGetResults);
    auto bindDeleteRequest = [this](auto& ctx) {
        auto it = ctx.pathParameters.find("id");
        if (it == ctx.pathParameters.end()) {
            sendNotFound(ctx);
            return;
        }
        try {
            handleDeleteRequest(ctx, std::stoi(it->second));
        } catch (...) {
            sendNotFound(ctx);
        }
    };
    router.addRoute("DELETE", "/deleteRequest/:id", bindDeleteRequest);
    router.addRoute("DELETE", "/api/deleteRequest/:id", bindDeleteRequest);

    auto bindDeleteResult = [this](auto& ctx) {
        auto it = ctx.pathParameters.find("id");
        if (it == ctx.pathParameters.end()) {
            sendNotFound(ctx);
            return;
        }
        try {
            handleDeleteResult(ctx, std::stoi(it->second));
        } catch (...) {
            sendNotFound(ctx);
        }
    };
    router.addRoute("DELETE", "/deleteResult/:id", bindDeleteResult);
    router.addRoute("DELETE", "/api/deleteResult/:id", bindDeleteResult);

    auto bindGetResult = [this](auto& ctx) {
        auto it = ctx.pathParameters.find("id");
        if (it == ctx.pathParameters.end()) {
            sendNotFound(ctx);
            return;
        }
        try {
            handleGetResult(ctx, std::stoi(it->second));
        } catch (...) {
            sendNotFound(ctx);
        }
    };
    router.addRoute("GET", "/getResult/:id", bindGetResult);
    router.addRoute("GET", "/api/getResult/:id", bindGetResult);

    auto bindGetBuyers = [this](auto& ctx) { handleGetBuyers(ctx); };
    router.addRoute("GET", "/getBuyer", bindGetBuyers);
    router.addRoute("GET", "/api/getBuyer", bindGetBuyers);
}

void QueryController::handlePending(quickgrab::server::RequestContext& ctx) {
    try {
        auto pending = queryService_.listPending(20);
        boost::json::array payload;
        payload.reserve(pending.size());
        for (const auto& request : pending) {
            payload.emplace_back(requestToJson(request));
        }
        sendJsonResponse(ctx, payload);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"加载待处理请求失败: "} + ex.what());
        sendServerError(ctx, "数据库查询失败");
    }
}

void QueryController::handleGetRequests(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto keywordIt = params.find("keyword");
    std::optional<std::string> keyword;
    if (keywordIt != params.end() && !keywordIt->second.empty()) {
        keyword = keywordIt->second;
    }
    auto buyerId = parseOptionalInt(params, "buyerId");
    auto type = parseOptionalInt(params, "type");
    auto status = parseOptionalInt(params, "status");
    std::string order = params.count("order") ? params["order"] : "id_desc";
    int offset = parseIntOrDefault(params, "offset", 0);
    int limit = parseIntOrDefault(params, "limit", 20);

    try {
        auto requests = queryService_.getRequestsByFilters(keyword, buyerId, type, status, order, offset, limit);
        boost::json::array payload;
        payload.reserve(requests.size());
        for (const auto& request : requests) {
            payload.emplace_back(requestToJson(request));
        }
        sendJsonResponse(ctx, payload);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"加载抢购请求失败: "} + ex.what());
        sendServerError(ctx, "数据库查询失败");
    }
}

void QueryController::handleGetResults(quickgrab::server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto keywordIt = params.find("keyword");
    std::optional<std::string> keyword;
    if (keywordIt != params.end() && !keywordIt->second.empty()) {
        keyword = keywordIt->second;
    }
    auto buyerId = parseOptionalInt(params, "buyerId");
    auto type = parseOptionalInt(params, "type");
    auto status = parseOptionalInt(params, "status");
    std::string order = params.count("order") ? params["order"] : "end_time_desc";
    int offset = parseIntOrDefault(params, "offset", 0);
    int limit = parseIntOrDefault(params, "limit", 20);

    try {
        auto results = queryService_.getResultsByFilters(keyword, buyerId, type, status, order, offset, limit);
        boost::json::array payload;
        payload.reserve(results.size());
        for (const auto& result : results) {
            payload.emplace_back(resultToJson(result));
        }
        sendJsonResponse(ctx, payload);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"加载抢购结果失败: "} + ex.what());
        sendServerError(ctx, "数据库查询失败");
    }
}

void QueryController::handleDeleteRequest(quickgrab::server::RequestContext& ctx, int requestId) {
    try {
        if (queryService_.deleteRequestById(requestId)) {
            sendJsonResponse(ctx, boost::json::object{});
        } else {
            sendServerError(ctx, "删除失败");
        }
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error,
                  "删除抢购请求出现异常 id=" + std::to_string(requestId) + " error=" + ex.what());
        sendServerError(ctx, "删除失败");
    }
}

void QueryController::handleDeleteResult(quickgrab::server::RequestContext& ctx, int resultId) {
    try {
        if (queryService_.deleteResultById(resultId)) {
            sendJsonResponse(ctx, boost::json::object{});
        } else {
            sendServerError(ctx, "删除失败");
        }
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error,
                  "删除抢购结果出现异常 id=" + std::to_string(resultId) + " error=" + ex.what());
        sendServerError(ctx, "删除失败");
    }
}

void QueryController::handleGetResult(quickgrab::server::RequestContext& ctx, int resultId) {
    try {
        auto result = queryService_.getResultById(resultId);
        if (!result) {
            sendNotFound(ctx);
            return;
        }
        sendJsonResponse(ctx, resultToJson(*result));
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error,
                  "查询抢购结果详情失败 id=" + std::to_string(resultId) + " error=" + ex.what());
        sendServerError(ctx, "数据库查询失败");
    }
}

void QueryController::handleGetBuyers(quickgrab::server::RequestContext& ctx) {
    try {
        auto buyers = queryService_.getAllBuyers();
        boost::json::array payload;
        payload.reserve(buyers.size());
        for (const auto& buyer : buyers) {
            payload.emplace_back(buyerToJson(buyer));
        }
        sendJsonResponse(ctx, payload);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"加载买家列表失败: "} + ex.what());
        sendServerError(ctx, "数据库查询失败");
    }
}

} // namespace quickgrab::controller
