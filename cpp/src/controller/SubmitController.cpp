#include "quickgrab/controller/SubmitController.hpp"

#include "quickgrab/model/Buyer.hpp"
#include "quickgrab/model/Request.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
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

boost::json::object makeStatus(int code, std::string message, std::string description = {}) {
    boost::json::object status;
    status["code"] = code;
    status["message"] = std::move(message);
    if (!description.empty()) {
        status["description"] = std::move(description);
    }
    return status;
}

boost::json::object wrapResponse(boost::json::object status, boost::json::object result) {
    boost::json::object response;
    response["status"] = std::move(status);
    response["result"] = std::move(result);
    return response;
}

std::string readString(const boost::json::object& object, const char* key) {
    if (auto it = object.if_contains(key); it) {
        if (it->is_string()) {
            return std::string(it->as_string().c_str());
        }
        if (it->is_int64()) {
            return std::to_string(it->as_int64());
        }
        if (it->is_double()) {
            return std::to_string(it->as_double());
        }
    }
    return {};
}

int readInt(const boost::json::object& object, const char* key, int fallback = 0) {
    if (auto it = object.if_contains(key); it) {
        if (it->is_int64()) {
            return static_cast<int>(it->as_int64());
        }
        if (it->is_double()) {
            return static_cast<int>(it->as_double());
        }
        if (it->is_string()) {
            try {
                return std::stoi(std::string(it->as_string().c_str()));
            } catch (const std::exception&) {
            }
        }
    }
    return fallback;
}

double readDouble(const boost::json::object& object, const char* key, double fallback = 0.0) {
    if (auto it = object.if_contains(key); it) {
        if (it->is_double()) {
            return it->as_double();
        }
        if (it->is_int64()) {
            return static_cast<double>(it->as_int64());
        }
        if (it->is_string()) {
            try {
                return std::stod(std::string(it->as_string().c_str()));
            } catch (const std::exception&) {
            }
        }
    }
    return fallback;
}

boost::json::value readJson(const boost::json::object& object, const char* key) {
    if (auto it = object.if_contains(key); it) {
        if (it->is_object() || it->is_array() || it->is_null()) {
            return *it;
        }
        if (it->is_string()) {
            try {
                return quickgrab::util::parseJson(std::string(it->as_string().c_str()));
            } catch (const std::exception&) {
            }
        }
    }
    return boost::json::object{};
}

std::chrono::system_clock::time_point parseDateTime(const boost::json::object& object, const char* key) {
    if (auto it = object.if_contains(key); it && it->is_string()) {
        std::string text(it->as_string().c_str());
        if (!text.empty()) {
            std::replace(text.begin(), text.end(), 'T', ' ');
            if (text.size() == 16) {
                text += ":00";
            }
            std::tm tm{};
            std::istringstream iss(text);
            iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (!iss.fail()) {
                auto time = std::mktime(&tm);
                if (time != -1) {
                    return std::chrono::system_clock::from_time_t(time);
                }
            } else {
                quickgrab::util::log(quickgrab::util::LogLevel::warn,
                                     std::string{"解析日期字段失败: "} + text);
            }
        }
    }
    return {};
}

boost::json::object sanitizeRequestPayload(boost::json::object payload) {
    auto normalize = [&](const char* key) {
        if (auto it = payload.if_contains(key); it) {
            auto parsed = ensureJsonObject(*it);
            if (!parsed.empty() || it->is_object()) {
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

    if (!payload.if_contains("deviceId")) {
        payload["deviceId"] = 1;
    }

    payload["status"] = 0;
    return payload;
}

model::Request buildRequestModel(const boost::json::object& payload) {
    model::Request request{};
    request.deviceId = readInt(payload, "deviceId", 1);
    request.buyerId = readInt(payload, "buyerId");
    request.threadId = readString(payload, "threadId");
    request.link = readString(payload, "link");
    request.cookies = readString(payload, "cookies");
    request.orderInfo = readJson(payload, "orderInfo");
    request.userInfo = readJson(payload, "userInfo");
    request.orderTemplate = readJson(payload, "orderTemplate");
    request.message = readString(payload, "message");
    request.idNumber = readString(payload, "idNumber");
    request.keyword = readString(payload, "keyword");
    request.startTime = parseDateTime(payload, "startTime");
    request.endTime = parseDateTime(payload, "endTime");
    request.quantity = readInt(payload, "quantity", 1);
    request.delay = readInt(payload, "delay");
    request.frequency = readInt(payload, "frequency");
    request.type = readInt(payload, "type");
    request.status = 0;
    if (auto it = payload.if_contains("orderParameters")) {
        if (it->is_string()) {
            request.orderParametersRaw = std::string(it->as_string().c_str());
        } else if (it->is_object() || it->is_array()) {
            request.orderParametersRaw = quickgrab::util::stringifyJson(*it);
        }
    }
    request.orderParameters = readJson(payload, "orderParameters");
    request.actualEarnings = readDouble(payload, "actualEarnings");
    request.estimatedEarnings = readDouble(payload, "estimatedEarnings");
    request.extension = readJson(payload, "extension");
    return request;
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
        if (delay->is_string()) {
            try {
                return std::stol(std::string(delay->as_string().c_str()));
            } catch (const std::exception&) {
            }
        }
    }
    return 0;
}

boost::json::object buyerToJson(const model::Buyer& buyer) {
    boost::json::object obj;
    obj["id"] = buyer.id;
    obj["username"] = buyer.username;
    obj["email"] = buyer.email;
    obj["accessLevel"] = buyer.accessLevel;
    return obj;
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
                auto endIt =
                    std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
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

void sendUnauthorized(quickgrab::server::RequestContext& ctx) {
    auto status = makeStatus(401, "ERROR", "未登录");
    auto response = wrapResponse(std::move(status), {});
    sendJsonResponse(ctx, boost::beast::http::status::unauthorized, response);
}

std::optional<service::AuthService::SessionInfo> authenticateRequest(service::AuthService& authService,
                                                                     quickgrab::server::RequestContext& ctx) {
    std::string token;
    if (auto header = ctx.request.find(boost::beast::http::field::cookie); header != ctx.request.end()) {
        auto cookies = parseCookies(std::string(header->value()));
        if (auto it = cookies.find(std::string(service::AuthService::kSessionCookie)); it != cookies.end()) {
            token = it->second;
        }
    }

    if (token.empty()) {
        return std::nullopt;
    }

    return authService.touchSession(token);
}

} // namespace

SubmitController::SubmitController(service::GrabService& grabService, service::AuthService& authService)
    : grabService_(grabService)
    , authService_(authService) {}

void SubmitController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/submitRequest", [this](auto& ctx) { handleSubmitRequest(ctx); });
}

void SubmitController::handleSubmitRequest(quickgrab::server::RequestContext& ctx) {
    auto session = authenticateRequest(authService_, ctx);
    if (!session) {
        sendUnauthorized(ctx);
        return;
    }

    if (session->rememberMe) {
        ctx.response.set(boost::beast::http::field::set_cookie, authService_.buildSessionCookie(*session));
    }

    try {
        auto json = boost::json::parse(ctx.request.body());
        if (!json.is_object()) {
            throw std::invalid_argument("请求必须是JSON对象");
        }

        auto payload = sanitizeRequestPayload(json.as_object());
        payload["buyerId"] = session->buyer.id;
        auto requestModel = buildRequestModel(payload);

        auto insertedId = grabService_.handleRequest(requestModel);
        if (!insertedId) {
            throw std::runtime_error("插入请求失败");
        }

        payload["id"] = *insertedId;
        long networkDelay = extractNetworkDelay(payload);

        boost::json::object result;
        result["networkDelay"] = networkDelay;
        result["request"] = payload;
        result["user"] = buyerToJson(session->buyer);

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    } catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(400, "ERROR", ex.what()), {});
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, response);
    }
}

} // namespace quickgrab::controller
