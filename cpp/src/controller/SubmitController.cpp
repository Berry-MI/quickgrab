#include "quickgrab/controller/SubmitController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <string>
#include <iostream>

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

boost::json::object buildRequestEcho(boost::json::object payload) {
    std::cout<<boost::json::serialize(payload)<<std::endl;

    auto normalize = [&](const char* key) {
        if (auto it = payload.if_contains(key); it) {
            auto parsed = ensureJsonObject(*it);
            if (!parsed.empty()) {
                payload[key] = std::move(parsed);
            }
        }
    };

    normalize("userInfo");
    normalize("orderTemplate");
    normalize("orderParameters");
    normalize("extension");

    if (!payload.if_contains("userInfo") || !payload.at("userInfo").is_object()) {
        boost::json::object userInfo{{"nickName", "未知"}, {"telephone", "未知"}};
        payload["userInfo"] = std::move(userInfo);
    } else {
        auto& info = payload.at("userInfo").as_object();
        if (!info.if_contains("nickName")) {
            info["nickName"] = "未知";
        }
        if (!info.if_contains("telephone")) {
            info["telephone"] = "未知";
        }
    }

    if (!payload.if_contains("id")) {
        static std::atomic<int> counter{1};
        payload["id"] = counter.fetch_add(1);
    }
    std::cout << boost::json::serialize(payload) << std::endl;
    return payload;
}

long extractNetworkDelay(const boost::json::object& request) {
    if (auto extension = request.if_contains("extension")) {
        auto ext = ensureJsonObject(*extension);
        if (auto value = ext.if_contains("networkDelay")) {
            if (value->is_int64()) {
                return static_cast<long>(value->as_int64());
            }
            if (value->is_double()) {
                return static_cast<long>(value->as_double());
            }
        }
    }
    if (auto delay = request.if_contains("delay")) {
        if (delay->is_int64()) {
            return static_cast<long>(delay->as_int64());
        }
        if (delay->is_double()) {
            return static_cast<long>(delay->as_double());
        }
    }
    return 0;
}

} // namespace

void SubmitController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/submitRequest", [this](auto& ctx) { handleSubmitRequest(ctx); });
}

void SubmitController::handleSubmitRequest(quickgrab::server::RequestContext& ctx) {
    try {
        auto json = boost::json::parse(ctx.request.body());
		
        std::cout << boost::json::serialize(json)<<std::endl;;
        if (!json.is_object()) {
            throw std::invalid_argument("请求体必须是JSON对象");
        }

        auto request = buildRequestEcho(json.as_object());
        long networkDelay = extractNetworkDelay(request);

        boost::json::object result;
        result["networkDelay"] = networkDelay;
        result["request"] = request;

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    } catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(400, ex.what()), {});
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, response);
    }
}

} // namespace quickgrab::controller
