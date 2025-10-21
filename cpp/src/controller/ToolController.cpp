#include "quickgrab/controller/ToolController.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/string_view.hpp>
#include <boost/json.hpp>

#include "quickgrab/util/HttpClient.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace quickgrab {
namespace controller {
namespace {

using quickgrab::server::RequestContext;

void sendJsonResponse(RequestContext& ctx,
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
    return std::string_view(begin, static_cast<std::size_t>(std::distance(begin, end)));
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
        value.push_back("0123456789abcdef"[dist(rng)]);
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

std::optional<boost::json::object> fetchItemInfo(util::HttpClient& httpClient,
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

    std::vector<util::HttpClient::Header> headers{{"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
                                                  {"Referer", "https://android.weidian.com"},
                                                  {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36"}};

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
    if (paramPos != std::string::npos) {
        auto start = paramPos + 6;
        auto end = link.find('&', start);
        std::string encoded = link.substr(start, end == std::string::npos ? std::string::npos : end - start);
        auto payload = percentDecode(encoded);
        if (!payload.empty()) {
            return parseJsonObject(payload);
        }
    }

    auto queryPos = link.find('?');
    if (queryPos == std::string::npos || queryPos + 1 >= link.size()) {
        return {};
    }

    std::string query = link.substr(queryPos + 1);
    std::string itemsParam;
    std::string sourceId;

    std::size_t start = 0;
    while (start < query.size()) {
        auto end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }
        auto token = query.substr(start, end - start);
        start = end + 1;
        if (token.empty()) {
            continue;
        }

        auto eq = token.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        auto key = token.substr(0, eq);
        auto value = token.substr(eq + 1);
        if (key == "items") {
            itemsParam = percentDecode(value);
        } else if (key == "source_id") {
            sourceId = percentDecode(value);
        }
    }

    if (itemsParam.empty()) {
        return {};
    }

    boost::json::array itemList;
    std::size_t itemStart = 0;
    while (itemStart < itemsParam.size()) {
        auto itemEnd = itemsParam.find(',', itemStart);
        if (itemEnd == std::string::npos) {
            itemEnd = itemsParam.size();
        }
        auto part = itemsParam.substr(itemStart, itemEnd - itemStart);
        itemStart = itemEnd + 1;
        if (part.empty()) {
            continue;
        }

        std::vector<std::string> pieces;
        std::size_t pieceStart = 0;
        while (pieceStart <= part.size()) {
            auto pieceEnd = part.find('_', pieceStart);
            if (pieceEnd == std::string::npos) {
                pieces.emplace_back(part.substr(pieceStart));
                break;
            }
            pieces.emplace_back(part.substr(pieceStart, pieceEnd - pieceStart));
            pieceStart = pieceEnd + 1;
        }

        if (pieces.size() < 4 || pieces[0].empty()) {
            continue;
        }

        boost::json::object item;
        item["item_id"] = pieces[0];

        try {
            auto quantity = !pieces[1].empty() ? std::stoll(pieces[1]) : 0LL;
            item["quantity"] = quantity;
        } catch (const std::exception&) {
            item["quantity"] = 0;
        }

        item["price_type"] = pieces[2];

        std::string skuId = pieces[3];
        if (skuId.empty() || skuId == "__") {
            skuId = "0";
        }
        item["item_sku_id"] = skuId;
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
    if (!sourceId.empty()) {
        payload["source_id"] = std::move(sourceId);
    }
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

ToolController::ToolController(util::HttpClient& httpClient)
    : httpClient_(httpClient) {}

void ToolController::registerRoutes(server::Router& router) {
    auto bindGetNote = [this](auto& ctx) { handleGetNote(ctx); };
    router.addRoute("POST", "/getNote", bindGetNote, { .requireAuth = true });
    router.addRoute("POST", "/api/getNote", bindGetNote, { .requireAuth = true });

    auto bindFetchItemInfo = [this](auto& ctx) { handleFetchItemInfo(ctx); };
    router.addRoute("POST", "/fetchItemInfo", bindFetchItemInfo, { .requireAuth = true });
    router.addRoute("POST", "/api/fetchItemInfo", bindFetchItemInfo, { .requireAuth = true });

    auto bindCheckCookies = [this](auto& ctx) { handleCheckCookies(ctx); };
    router.addRoute("GET", "/checkCookiesValidity", bindCheckCookies, { .requireAuth = true });
    router.addRoute("GET", "/api/checkCookiesValidity", bindCheckCookies, { .requireAuth = true });

    auto bindCheckLatency = [this](auto& ctx) { handleCheckLatency(ctx); };
    router.addRoute("POST", "/checkLatency", bindCheckLatency, { .requireAuth = true });
    router.addRoute("POST", "/api/checkLatency", bindCheckLatency, { .requireAuth = true });
}

void ToolController::handleGetNote(server::RequestContext& ctx) {
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

void ToolController::handleFetchItemInfo(server::RequestContext& ctx) {
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
        if (param.empty()) {
            throw std::invalid_argument("获取参数失败");
        }

        int categoryCount = 0;
        int stockQuantity = 0;
        bool hasExpireDate = false;
        bool isFutureSold = false;
        std::int64_t futureSoldTime = 0;
        std::string itemId;
        std::optional<std::int64_t> skuId;

        if (auto listIt = param.if_contains("item_list"); listIt && listIt->is_array()) {
            const auto& list = listIt->as_array();
            categoryCount = static_cast<int>(list.size());
            if (!list.empty()) {
                const auto& item = list.front();
                if (item.is_object()) {
                    const auto& obj = item.as_object();
                    if (auto idIt = obj.if_contains("item_id")) {
                        if (idIt->is_string()) {
                            itemId = std::string(idIt->as_string().c_str());
                        } else if (auto idNum = readInt64(*idIt)) {
                            itemId = std::to_string(*idNum);
                        }
                    }
                    if (auto skuIt = obj.if_contains("item_sku_id")) {
                        skuId = readInt64(*skuIt);
                    }
                    if (auto convey = obj.if_contains("item_convey_info"); convey && convey->is_object()) {
                        if (auto valid = convey->as_object().if_contains("valid_date_info")) {
                            hasExpireDate = valid->is_object();
                        }
                    }
                    if (auto stockNode = obj.if_contains("stock")) {
                        if (auto stockVal = readInt64(*stockNode)) {
                            stockQuantity = static_cast<int>(*stockVal);
                        }
                    }
                }
            }
        }

        if (itemId.empty()) {
            throw std::invalid_argument("缺少商品ID");
        }

        auto itemInfo = fetchItemInfo(httpClient_, itemId, cookieIt->second);
        if (!itemInfo) {
            throw std::runtime_error("获取商品信息失败");
        }

        const auto& model = *itemInfo;
        const boost::json::object* itemInfoObj = nullptr;
        if (auto infoIt = model.if_contains("itemInfo"); infoIt && infoIt->is_object()) {
            itemInfoObj = &infoIt->as_object();
        }

        if (skuId && *skuId != 0) {
            auto skuProps = model.if_contains("skuProperties");
            if (!skuProps || !skuProps->is_object()) {
                throw std::runtime_error("未找到SKU信息");
            }
            auto skuList = skuProps->as_object().if_contains("sku");
            if (!skuList || !skuList->is_array()) {
                throw std::runtime_error("未找到SKU信息");
            }
            bool matched = false;
            for (const auto& skuValue : skuList->as_array()) {
                if (!skuValue.is_object()) {
                    continue;
                }
                const auto& skuObj = skuValue.as_object();
                auto idNode = skuObj.if_contains("id");
                if (!idNode) {
                    continue;
                }
                auto idValue = readInt64(*idNode);
                if (!idValue || *idValue != *skuId) {
                    continue;
                }
                if (auto stockNode = skuObj.if_contains("stock")) {
                    if (auto stockVal = readInt64(*stockNode)) {
                        stockQuantity = static_cast<int>(*stockVal);
                    }
                }
                matched = true;
                break;
            }
            if (!matched) {
                throw std::runtime_error("未找到对应的SKU信息");
            }
        } else if (itemInfoObj) {
            if (auto stockNode = itemInfoObj->if_contains("stock")) {
                if (auto stockVal = readInt64(*stockNode)) {
                    stockQuantity = static_cast<int>(*stockVal);
                }
            }
        }

        if (itemInfoObj) {
            if (auto ticketNode = itemInfoObj->if_contains("ticketItemInfo"); ticketNode && ticketNode->is_object()) {
                if (auto expireNode = ticketNode->as_object().if_contains("expireDate")) {
                    if ((expireNode->is_string() && !std::string(expireNode->as_string().c_str()).empty()) ||
                        (expireNode->is_object() && !expireNode->as_object().empty())) {
                        hasExpireDate = true;
                    }
                }
            }

            if (auto flagNode = itemInfoObj->if_contains("flag"); flagNode && flagNode->is_object()) {
                if (auto futureNode = flagNode->as_object().if_contains("isFutureSold")) {
                    if (auto future = readBool(*futureNode)) {
                        isFutureSold = *future;
                    }
                }
            }

            if (auto futureTimeNode = itemInfoObj->if_contains("futureSoldTime")) {
                if (auto ts = readInt64(*futureTimeNode)) {
                    futureSoldTime = *ts;
                }
            }
        }

        if (!isFutureSold) {
            futureSoldTime = 0;
        }

        int delayIncrement = calculateDelayIncrementWithJitter(stockQuantity);

        boost::json::object result;
        result["delayIncrement"] = delayIncrement;
        result["hasExpireDate"] = hasExpireDate;
        result["isFutureSold"] = isFutureSold;
        result["categoryCount"] = categoryCount;
        if (isFutureSold && futureSoldTime > 0) {
            result["futureSoldTime"] = futureSoldTime;
        }

        auto response = wrapResponse(makeStatus(200, "OK"), std::move(result));
        sendJsonResponse(ctx, boost::beast::http::status::ok, response);
    } catch (const std::exception& ex) {
        auto response = wrapResponse(makeStatus(400, ex.what()), {});
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, response);
    }
}

void ToolController::handleCheckCookies(server::RequestContext& ctx) {
    auto params = parseQueryParameters(ctx.request.target());
    auto it = params.find("cookies");
    bool valid = it != params.end() && looksLikeValidCookie(it->second);

    boost::json::object body;
    body["message"] = valid ? "Cookies有效" : "Cookies无效";
    sendJsonResponse(ctx, boost::beast::http::status::ok, std::move(body));
}

void ToolController::handleCheckLatency(server::RequestContext& ctx) {
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

} // namespace controller
} // namespace quickgrab

