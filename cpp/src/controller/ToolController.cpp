#include "quickgrab/controller/ToolController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "quickgrab/util/HttpClient.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

std::string urlEncode(const std::string& value) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << static_cast<char>(c);
        } else if (c == ' ') {
            oss << '+';
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            oss << std::setw(0);
        }
    }
    return oss.str();
}

std::string_view trimView(std::string_view view) {
    auto begin = view.begin();
    auto end = view.end();

    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    return std::string_view(
        view.data() + std::distance(view.begin(), begin),
        static_cast<std::size_t>(std::distance(begin, end))
    );
}

std::unordered_map<std::string, std::string> parseCookieString(const std::string& cookieStr) {
    std::unordered_map<std::string, std::string> cookies;
    std::size_t start = 0;
    while (start < cookieStr.size()) {
        auto end = cookieStr.find(';', start);
        if (end == std::string::npos) {
            end = cookieStr.size();
        }
        auto token = std::string_view(cookieStr.data() + start, end - start);
        start = end + 1;
        if (token.empty()) {
            continue;
        }
        auto eq = token.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        auto key = std::string(trimView(token.substr(0, eq)));
        auto value = std::string(trimView(token.substr(eq + 1)));
        if (!key.empty()) {
            cookies.emplace(std::move(key), std::move(value));
        }
    }
    return cookies;
}

std::string randomHex(std::size_t length) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    std::string value;
    value.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        int v = dist(rng);
        value.push_back("0123456789abcdef"[v]);
    }
    return value;
}

std::string generateAppVersion() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> first(0, 9);
    std::uniform_int_distribution<int> second(0, 19);
    std::uniform_int_distribution<int> third(0, 99);
    std::ostringstream oss;
    oss << first(rng) << '.' << second(rng) << '.' << third(rng);
    return oss.str();
}

std::string generateBrand() {
    static const std::vector<std::string> brands{"Xiaomi", "HUAWEI", "OPPO", "vivo", "Samsung", "OnePlus"};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, brands.size() - 1);
    return brands[dist(rng)];
}

std::string generateBuildNumber() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> month(1, 12);
    std::uniform_int_distribution<int> day(1, 28);
    std::uniform_int_distribution<int> seq(0, 999999);
    std::ostringstream oss;
    oss << "2025" << std::setw(2) << std::setfill('0') << month(rng)
        << std::setw(2) << std::setfill('0') << day(rng)
        << std::setw(6) << std::setfill('0') << seq(rng);
    return oss.str();
}

std::string generateChannel() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 9);
    return "100" + std::to_string(dist(rng));
}

std::string generateDiskCapacity() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> totalDist(32, 160);
    int total = totalDist(rng);
    std::uniform_int_distribution<int> usedDist(0, std::max(total - 1, 1));
    int used = usedDist(rng);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << static_cast<double>(used) << "GB/" << static_cast<double>(total) << "GB";
    return oss.str();
}

std::string generateFeature() {
    return "E|F,H|F,P|F,R|T,W|F";
}

std::string generateScreenHeight() {
    static const int heights[] = {1920, 2160, 2400, 2560};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, std::size(heights) - 1);
    return std::to_string(heights[dist(rng)]);
}

std::string generateScreenWidth() {
    static const int widths[] = {1080, 1440, 1600};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, std::size(widths) - 1);
    return std::to_string(widths[dist(rng)]);
}

std::string generateLatitude() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(18.0, 38.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << dist(rng);
    return oss.str();
}

std::string generateLongitude() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(73.0, 123.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << dist(rng);
    return oss.str();
}

std::string generateMac() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) {
        if (i != 0) {
            oss << ':';
        }
        oss << std::setw(2) << dist(rng);
    }
    return oss.str();
}

std::string generateMachineModel() {
    static const std::vector<std::string> models{"aarch64", "arm64-v8a", "armeabi-v7a", "x86_64"};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, models.size() - 1);
    return models[dist(rng)];
}

std::string generateMemory() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> totalDist(4, 12);
    int total = totalDist(rng);
    std::uniform_int_distribution<int> usedDist(0, std::max(total - 1, 0));
    int used = usedDist(rng);
    std::ostringstream oss;
    oss << used * 1024 << 'M' << '/' << total * 1024 << 'M';
    return oss.str();
}

std::string generateMid() {
    static const std::vector<std::string> mids{"MI_6", "MI_8", "MI_9", "MI_10", "MI_11", "MI_12"};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, mids.size() - 1);
    return mids[dist(rng)];
}

std::string generateOsVersion() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(24, 33);
    return std::to_string(dist(rng));
}

std::string generateWifiSsid() {
    static const std::vector<std::string> prefixes{"JXNUSDI", "CMCC", "ChinaNet", "TP-LINK", "HUAWEI"};
    static const std::vector<std::string> suffixes{"_stu", "_5G", "_guest", "_home", "_office"};
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> pDist(0, prefixes.size() - 1);
    std::uniform_int_distribution<std::size_t> sDist(0, suffixes.size() - 1);
    return "\"" + prefixes[pDist(rng)] + suffixes[sDist(rng)] + "\"";
}

boost::json::object buildContextFromCookies(const std::string& cookieStr) {
    auto cookieMap = parseCookieString(cookieStr);
    boost::json::object context;
    context["alt"] = "0.0";
    context["android_id"] = randomHex(16);
    context["app_status"] = "active";
    context["appid"] = "com.koudai.weidian.buyer";
    context["appv"] = generateAppVersion();
    context["brand"] = generateBrand();
    context["build"] = generateBuildNumber();
    context["channel"] = generateChannel();
    context["cuid"] = randomHex(32);
    context["disk_capacity"] = generateDiskCapacity();
    context["feature"] = generateFeature();
    context["h"] = generateScreenHeight();
    context["iccid"] = "";
    context["imei"] = "";
    context["imsi"] = "";
    context["lat"] = generateLatitude();
    context["lon"] = generateLongitude();
    context["mac"] = generateMac();
    context["machine_model"] = generateMachineModel();
    context["memory"] = generateMemory();
    context["mid"] = generateMid();
    context["mobile_station"] = "0";
    context["net_subtype"] = "0_";
    context["network"] = "WIFI";
    context["oaid"] = randomHex(16);
    context["os"] = generateOsVersion();
    context["platform"] = "android";
    context["serial_num"] = "";
    context["w"] = generateScreenWidth();
    context["wmac"] = generateMac();
    context["wssid"] = generateWifiSsid();
    context["is_login"] = cookieMap.contains("is_login") ? cookieMap["is_login"] : "1";
    context["login_token"] = cookieMap.contains("login_token") ? cookieMap["login_token"] : "";
    context["duid"] = cookieMap.contains("duid") ? cookieMap["duid"] : "";
    context["uid"] = cookieMap.contains("uid") ? cookieMap["uid"] : "";
    context["shop_id"] = cookieMap.contains("duid") ? cookieMap["duid"] : "";
    context["suid"] = randomHex(32);
    context["androidVersion"] = "12";
    context["deviceBrand"] = context["brand"];
    context["deviceModel"] = context["mid"];
    return context;
}

std::optional<std::int64_t> readInt64(const boost::json::value& value) {
    if (value.is_int64()) {
        return value.as_int64();
    }
    if (value.is_uint64()) {
        return static_cast<std::int64_t>(value.as_uint64());
    }
    if (value.is_double()) {
        return static_cast<std::int64_t>(value.as_double());
    }
    if (value.is_string()) {
        try {
            return std::stoll(std::string(value.as_string().c_str()));
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}

std::optional<bool> readBool(const boost::json::value& value) {
    if (value.is_bool()) {
        return value.as_bool();
    }
    if (value.is_string()) {
        auto str = std::string(value.as_string().c_str());
        if (str == "true" || str == "1") {
            return true;
        }
        if (str == "false" || str == "0") {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<boost::json::object> fetchItemInfo(quickgrab::util::HttpClient& httpClient,
                                                 const std::string& itemId,
                                                 const std::string& cookies) {
    auto context = buildContextFromCookies(cookies);
    boost::json::object paramObj;
    paramObj["adsk"] = "";
    paramObj["itemId"] = itemId;

    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::string url = "https://thor.weidian.com/detailmjb/getItemInfo/1.4?timestamp=" +
                      std::to_string(timestamp) +
                      "&context=" + urlEncode(boost::json::serialize(context)) +
                      "&param=" + urlEncode(boost::json::serialize(paramObj));

    std::vector<quickgrab::util::HttpClient::Header> headers{{"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
                                                             {"Referer", "https://android.weidian.com"},
                                                             {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76"}};

    auto response = httpClient.fetch("GET",
                                     url,
                                     headers,
                                     "",
                                     "",
                                     std::chrono::seconds{30},
                                     false,
                                     5,
                                     nullptr,
                                     false);

    auto body = boost::json::parse(response.body());
    if (!body.is_object()) {
        return std::nullopt;
    }
    const auto& obj = body.as_object();
    auto statusIt = obj.if_contains("status");
    if (!statusIt || !statusIt->is_object()) {
        return std::nullopt;
    }
    auto messageIt = statusIt->as_object().if_contains("message");
    if (!messageIt || !messageIt->is_string()) {
        return std::nullopt;
    }
    if (messageIt->as_string() != "OK") {
        return std::nullopt;
    }
    auto resultIt = obj.if_contains("result");
    if (!resultIt || !resultIt->is_object()) {
        return std::nullopt;
    }
    auto modelIt = resultIt->as_object().if_contains("defaultModel");
    if (!modelIt || !modelIt->is_object()) {
        return std::nullopt;
    }
    return modelIt->as_object();
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

boost::json::object parseLinkPayload(const std::string& link) {
    auto queryPos = link.find('?');
    if (queryPos == std::string::npos) {
        return {};
    }

    auto params = parseFormUrlEncoded(link.substr(queryPos + 1));
    auto itemsIt = params.find("items");
    auto sourceIt = params.find("source_id");
    if (itemsIt == params.end() || sourceIt == params.end()) {
        return {};
    }

    boost::json::array itemList;
    std::string_view itemsView(itemsIt->second);
    std::size_t start = 0;
    while (start <= itemsView.size()) {
        auto end = itemsView.find(',', start);
        if (end == std::string_view::npos) {
            end = itemsView.size();
        }
        auto token = itemsView.substr(start, end - start);
        start = end + 1;
        if (token.empty()) {
            continue;
        }

        std::vector<std::string> parts;
        std::size_t partStart = 0;
        while (partStart <= token.size()) {
            auto partEnd = token.find('_', partStart);
            if (partEnd == std::string_view::npos) {
                parts.emplace_back(token.substr(partStart));
                break;
            }
            parts.emplace_back(token.substr(partStart, partEnd - partStart));
            partStart = partEnd + 1;
        }

        if (parts.size() < 4) {
            continue;
        }

        int quantity = 0;
        try {
            quantity = std::stoi(parts[1]);
        } catch (const std::exception&) {
            continue;
        }

        boost::json::object item;
        item["item_id"] = parts[0];
        item["quantity"] = quantity;
        item["price_type"] = parts[2];
        auto skuId = parts[3];
        if (skuId.empty() || skuId == "__") {
            item["item_sku_id"] = "0";
        } else {
            item["item_sku_id"] = skuId;
        }
        item["item_type"] = "0";
        item["use_installment"] = 1;
        itemList.emplace_back(std::move(item));
    }

    if (itemList.empty()) {
        return {};
    }

    boost::json::object payload;
    payload["buyer"] = boost::json::object{};
    payload["channel"] = "maijiaban";
    payload["item_list"] = std::move(itemList);
    payload["source_id"] = sourceIt->second;
    return payload;
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

ToolController::ToolController(quickgrab::util::HttpClient& httpClient)
    : httpClient_(httpClient) {}

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
        // 1) 取表单参数
        auto form = parseFormUrlEncoded(ctx.request.body());
        const auto linkIt = form.find("link");
        const auto cookieIt = form.find("cookies");
        if (linkIt == form.end() || linkIt->second.empty() ||
            cookieIt == form.end() || cookieIt->second.empty()) {
            // Java 的习惯：参数无法解析统一走 201
            auto resp = wrapResponse(makeStatus(201, "获取参数失败"), {});
            sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
            return;
        }

        // 2) link → paramNode（等价于 Java 的 CommonUtil.convertLinkToJson）
        boost::json::object paramNode = parseLinkPayload(linkIt->second);
        if (paramNode.empty()) {
            auto resp = wrapResponse(makeStatus(201, "获取参数失败"), {});
            sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
            return;
        }

        // 3) 读取 itemId 与 skuId（取第一项）
        std::string itemId;
        std::int64_t skuId = 0;
        int categoryCount = 0;

        if (auto* list = paramNode.if_contains("item_list"); list && list->is_array() && !list->as_array().empty()) {
            const auto& first = list->as_array().front();
            categoryCount = static_cast<int>(list->as_array().size());
            if (first.is_object()) {
                const auto& obj = first.as_object();
                // item_id
                if (auto* id = obj.if_contains("item_id")) {
                    if (id->is_string()) {
                        itemId = std::string(id->as_string().c_str());
                    }
                    else if (id->is_int64()) {
                        itemId = std::to_string(id->as_int64());
                    }
                }
                // item_sku_id
                if (auto* sid = obj.if_contains("item_sku_id")) {
                    if (sid->is_int64()) skuId = sid->as_int64();
                    else if (sid->is_string()) {
                        try { skuId = std::stoll(std::string(sid->as_string().c_str())); }
                        catch (...) {}
                    }
                }
            }
        }

        if (itemId.empty()) {
            auto resp = wrapResponse(makeStatus(201, "获取参数失败"), {});
            sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
            return;
        }

        // 4) 获取商品信息（等价于 Java 的 NetworkUtil.getItemInfo）
        auto itemInfoOpt = fetchItemInfo(httpClient_, itemId, cookieIt->second);
        if (!itemInfoOpt) {
            auto resp = wrapResponse(makeStatus(202, "获取商品信息失败"), {});
            sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
            return;
        }
        const auto& dataObj = *itemInfoOpt;

        // 5) 计算库存
        int stockQuantity = 0;
        if (skuId != 0) {
            // skuProperties.sku 数组里查找 id = skuId
            const auto* skuProps = dataObj.if_contains("skuProperties");
            const auto* skuList = (skuProps && skuProps->is_object())
                ? skuProps->as_object().if_contains("sku")
                : nullptr;
            if (!skuList || !skuList->is_array()) {
                auto resp = wrapResponse(makeStatus(203, "未找到对应的SKU信息"), {});
                sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
                return;
            }
            bool matched = false;
            for (const auto& sku : skuList->as_array()) {
                if (!sku.is_object()) continue;
                const auto& so = sku.as_object();
                std::int64_t idVal = 0;
                if (auto* id = so.if_contains("id")) {
                    if (id->is_int64()) idVal = id->as_int64();
                    else if (id->is_string()) {
                        try { idVal = std::stoll(std::string(id->as_string().c_str())); }
                        catch (...) {}
                    }
                }
                if (idVal == skuId) {
                    if (auto* st = so.if_contains("stock"); st && st->is_int64()) {
                        stockQuantity = static_cast<int>(st->as_int64());
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                auto resp = wrapResponse(makeStatus(203, "未找到对应的SKU信息"), {});
                sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
                return;
            }
        }
        else {
            // itemInfo.stock
            const auto* itemInfo = dataObj.if_contains("itemInfo");
            if (!itemInfo || !itemInfo->is_object()) {
                auto resp = wrapResponse(makeStatus(202, "获取商品信息失败"), {});
                sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
                return;
            }
            const auto& infoObj = itemInfo->as_object();
            if (auto* st = infoObj.if_contains("stock"); st && st->is_int64()) {
                stockQuantity = static_cast<int>(st->as_int64());
            }
            else {
                // 与 Java 行为保持：若缺失也视为信息不足
                auto resp = wrapResponse(makeStatus(202, "获取商品信息失败"), {});
                sendJsonResponse(ctx, boost::beast::http::status::bad_request, resp);
                return;
            }
        }

        // 6) 计算延时增量
        int delayIncrement = calculateDelayIncrementWithJitter(stockQuantity);

        // 7) 票务/有效期字段
        bool hasExpireDate = false;
        boost::json::value expireDateParsed; // 可选：解析后的结构
        const auto* itemInfo = dataObj.if_contains("itemInfo");
        if (itemInfo && itemInfo->is_object()) {
            const auto& infoObj = itemInfo->as_object();
            if (auto* ticket = infoObj.if_contains("ticketItemInfo"); ticket && ticket->is_object()) {
                const auto& t = ticket->as_object();
                if (auto* expire = t.if_contains("expireDate")) {
                    hasExpireDate = true;
                    // Java: CommonUtil.parseExpireDate(expireDate)
                    if (expire->is_string()) {
                        std::string expStr = std::string(expire->as_string().c_str());
                        // 如果你有解析工具函数，替换下面这行：
                        // expireDateParsed = quickgrab::util::parseExpireDate(expStr);
                        // 这里先原样返回字符串作为兼容：
                        expireDateParsed = boost::json::value(expStr);
                    }
                    else if (expire->is_object()) {
                        expireDateParsed = *expire; // 已经是对象，直接透传
                    }
                }
            }
        }

        // 8) 预售字段
        bool isFutureSold = false;
        std::int64_t futureSoldTime = 0;
        if (itemInfo && itemInfo->is_object()) {
            const auto& infoObj = itemInfo->as_object();
            if (auto* flag = infoObj.if_contains("flag"); flag && flag->is_object()) {
                if (auto* fs = flag->as_object().if_contains("isFutureSold"); fs) {
                    if (fs->is_bool()) isFutureSold = fs->as_bool();
                    else if (fs->is_int64()) isFutureSold = (fs->as_int64() != 0);
                }
            }
            if (isFutureSold) {
                if (auto* ts = infoObj.if_contains("futureSoldTime"); ts && ts->is_int64()) {
                    futureSoldTime = ts->as_int64();
                }
            }
        }

        // 9) 组织结果（与 Java 字段一致）
        boost::json::object result;
        result["delayIncrement"] = delayIncrement;
        result["hasExpireDate"] = hasExpireDate;
        if (hasExpireDate && !expireDateParsed.is_null()) {
            result["expireDate"] = expireDateParsed; // 与 Java: resultNode.set("expireDate", item_convey_info)
        }
        result["isFutureSold"] = isFutureSold;
        if (isFutureSold && futureSoldTime > 0) {
            result["futureSoldTime"] = futureSoldTime;
        }
        result["categoryCount"] = categoryCount;
        if (isFutureSold && futureSoldTime > 0) {
            result["futureSoldTime"] = futureSoldTime;
        }

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    }
    catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(204, ex.what()), {});
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
