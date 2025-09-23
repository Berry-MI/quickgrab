#include "quickgrab/controller/ProxyController.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"
#include "quickgrab/util/WeidianParser.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quickgrab::controller {

namespace {

constexpr char kDesktopUA[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76";
constexpr char kMobileUA[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1";

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
            std::string key = urlDecode(token.substr(0, eq));
            std::string value = urlDecode(token.substr(eq + 1));
            params.emplace(std::move(key), std::move(value));
        } else {
            params.emplace(urlDecode(token), "");
        }
        start = end + 1;
    }
    return params;
}

std::string randomBoundary() {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    std::string boundary = "----QuickGrabBoundary";
    for (int i = 0; i < 16; ++i) {
        boundary.push_back("0123456789ABCDEF"[dist(rng)]);
    }
    return boundary;
}

std::optional<std::string> extractMultipartTextField(const std::string& body,
                                                     const std::string& boundary,
                                                     const std::string& fieldName) {
    std::string marker = "name=\"" + fieldName + "\"";
    std::string boundaryMarker = "--" + boundary;
    std::size_t pos = 0;
    while ((pos = body.find(boundaryMarker, pos)) != std::string::npos) {
        pos += boundaryMarker.size();
        if (body.compare(pos, 2, "--") == 0) {
            break;
        }
        if (body.compare(pos, 2, "\r\n") == 0) {
            pos += 2;
        }
        auto headerEnd = body.find("\r\n\r\n", pos);
        if (headerEnd == std::string::npos) {
            break;
        }
        auto headersBlock = body.substr(pos, headerEnd - pos);
        if (headersBlock.find(marker) == std::string::npos) {
            pos = headerEnd + 4;
            continue;
        }
        auto valueStart = headerEnd + 4;
        auto nextBoundary = body.find(boundaryMarker, valueStart);
        if (nextBoundary == std::string::npos) {
            break;
        }
        auto valueEnd = nextBoundary;
        if (valueEnd >= 2 && body[valueEnd - 2] == '\r' && body[valueEnd - 1] == '\n') {
            valueEnd -= 2;
        }
        return body.substr(valueStart, valueEnd - valueStart);
    }
    return std::nullopt;
}

struct MultipartFilePart {
    std::string filename;
    std::string contentType;
    std::string data;
};

std::optional<MultipartFilePart> extractMultipartFile(const std::string& body,
                                                      const std::string& boundary,
                                                      const std::string& fieldName) {
    std::string marker = "name=\"" + fieldName + "\"";
    std::string boundaryMarker = "--" + boundary;
    std::size_t pos = 0;
    while ((pos = body.find(boundaryMarker, pos)) != std::string::npos) {
        pos += boundaryMarker.size();
        if (body.compare(pos, 2, "--") == 0) {
            break;
        }
        if (body.compare(pos, 2, "\r\n") == 0) {
            pos += 2;
        }
        auto headerEnd = body.find("\r\n\r\n", pos);
        if (headerEnd == std::string::npos) {
            break;
        }
        auto headersBlock = body.substr(pos, headerEnd - pos);
        if (headersBlock.find(marker) == std::string::npos) {
            pos = headerEnd + 4;
            continue;
        }
        MultipartFilePart part;
        auto filenamePos = headersBlock.find("filename=\"");
        if (filenamePos != std::string::npos) {
            filenamePos += 10;
            auto filenameEnd = headersBlock.find('"', filenamePos);
            if (filenameEnd != std::string::npos) {
                part.filename = headersBlock.substr(filenamePos, filenameEnd - filenamePos);
            }
        }
        auto contentTypePos = headersBlock.find("Content-Type:");
        if (contentTypePos != std::string::npos) {
            contentTypePos += std::string("Content-Type:").size();
            while (contentTypePos < headersBlock.size() && std::isspace(static_cast<unsigned char>(headersBlock[contentTypePos]))) {
                ++contentTypePos;
            }
            auto contentTypeEnd = headersBlock.find('\r', contentTypePos);
            if (contentTypeEnd != std::string::npos) {
                part.contentType = headersBlock.substr(contentTypePos, contentTypeEnd - contentTypePos);
            }
        }
        auto dataStart = headerEnd + 4;
        auto nextBoundary = body.find(boundaryMarker, dataStart);
        if (nextBoundary == std::string::npos) {
            break;
        }
        auto dataEnd = nextBoundary;
        if (dataEnd >= 2 && body[dataEnd - 2] == '\r' && body[dataEnd - 1] == '\n') {
            dataEnd -= 2;
        }
        part.data.assign(body.data() + dataStart, body.data() + dataEnd);
        return part;
    }
    return std::nullopt;
}

void sendJsonResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const std::string& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = body;
    ctx.response.prepare_payload();
}

void sendTextResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const std::string& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
    ctx.response.body() = body;
    ctx.response.prepare_payload();
}

std::optional<std::string> getHeaderValue(const quickgrab::server::RequestContext::HttpRequest& request,
                                          boost::beast::http::field field) {
    auto it = request.find(field);
    if (it == request.end()) {
        return std::nullopt;
    }
    return std::string(it->value());
}

std::optional<std::string> parseBoundary(const quickgrab::server::RequestContext::HttpRequest& request) {
    auto contentType = getHeaderValue(request, boost::beast::http::field::content_type);
    if (!contentType) {
        return std::nullopt;
    }
    auto pos = contentType->find("boundary=");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::string boundary = contentType->substr(pos + 9);
    auto semicolon = boundary.find(';');
    if (semicolon != std::string::npos) {
        boundary = boundary.substr(0, semicolon);
    }
    if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    return boundary;
}

std::string trim(const std::string& value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

bool parseBoolString(const std::string& value) {
    auto trimmed = trim(value);
    if (trimmed.empty()) {
        return false;
    }
    std::string lowered;
    lowered.reserve(trimmed.size());
    std::transform(trimmed.begin(), trimmed.end(), std::back_inserter(lowered), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "y") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off" || lowered == "n") {
        return false;
    }
    return false;
}

bool shouldUseProxy(const std::unordered_map<std::string, std::string>& queryParams,
                    const std::unordered_map<std::string, std::string>& formParams) {
    auto resolveFlag = [](const auto& container, const char* key) -> std::optional<bool> {
        if (auto it = container.find(key); it != container.end()) {
            return parseBoolString(it->second);
        }
        return std::nullopt;
    };

    if (auto flag = resolveFlag(formParams, "useProxy")) {
        return *flag;
    }
    if (auto flag = resolveFlag(queryParams, "useProxy")) {
        return *flag;
    }
    if (auto flag = resolveFlag(formParams, "proxy")) {
        return *flag;
    }
    if (auto flag = resolveFlag(queryParams, "proxy")) {
        return *flag;
    }

    auto hasAffinity = [](const auto& container, const char* key) {
        if (auto it = container.find(key); it != container.end()) {
            return !trim(it->second).empty();
        }
        return false;
    };

    if (hasAffinity(formParams, "proxyAffinity") || hasAffinity(queryParams, "proxyAffinity")) {
        return true;
    }
    if (hasAffinity(formParams, "affinity") || hasAffinity(queryParams, "affinity")) {
        return true;
    }

    return false;
}

std::string hostFromUrl(const std::string& value) {
    auto schemePos = value.find("://");
    std::size_t hostStart = schemePos == std::string::npos ? 0 : schemePos + 3;
    auto pathPos = value.find('/', hostStart);
    std::string host = value.substr(hostStart, pathPos == std::string::npos ? std::string::npos : pathPos - hostStart);
    auto atPos = host.find('@');
    if (atPos != std::string::npos) {
        host = host.substr(atPos + 1);
    }
    auto colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        host = host.substr(0, colonPos);
    }
    return trim(host);
}

std::string resolveAffinityKey(const std::unordered_map<std::string, std::string>& queryParams,
                               const std::unordered_map<std::string, std::string>& formParams,
                               const std::string& fallback) {
    auto resolveFrom = [](const auto& container, std::initializer_list<const char*> keys) -> std::optional<std::string> {
        for (auto key : keys) {
            if (auto it = container.find(key); it != container.end()) {
                auto trimmed = trim(it->second);
                if (!trimmed.empty()) {
                    return trimmed;
                }
            }
        }
        return std::nullopt;
    };

    auto resolve = [&](std::initializer_list<const char*> keys) -> std::optional<std::string> {
        if (auto value = resolveFrom(formParams, keys)) {
            return value;
        }
        if (auto value = resolveFrom(queryParams, keys)) {
            return value;
        }
        return std::nullopt;
    };

    if (auto affinity = resolve({"proxyAffinity", "affinity"})) {
        return *affinity;
    }
    if (auto thread = resolve({"threadId", "thread_id"})) {
        return *thread;
    }
    if (auto device = resolve({"deviceId", "device_id"})) {
        return *device;
    }
    if (auto buyer = resolve({"buyerId", "buyer_id"})) {
        return *buyer;
    }
    if (auto cookie = resolve({"cookie"})) {
        return *cookie;
    }
    if (auto phone = resolve({"phone"})) {
        return *phone;
    }
    if (auto url = resolve({"url"})) {
        auto host = hostFromUrl(*url);
        if (!host.empty()) {
            return host;
        }
        return *url;
    }

    auto trimmedFallback = trim(fallback);
    if (trimmedFallback.empty()) {
        return "default";
    }
    auto host = hostFromUrl(trimmedFallback);
    if (!host.empty()) {
        return host;
    }
    return trimmedFallback;
}
void proxyResponseToContext(quickgrab::server::RequestContext& ctx,
                            const HttpClient::HttpResponse& response) {
    ctx.response.result(response.result());
    auto contentTypeIt = response.base().find(boost::beast::http::field::content_type);
    if (contentTypeIt != response.base().end()) {
        ctx.response.set(boost::beast::http::field::content_type, contentTypeIt->value());
    }
    ctx.response.body() = response.body();
    ctx.response.prepare_payload();
}

} // namespace

ProxyController::ProxyController(service::ProxyService& proxies,
                                 util::HttpClient& client)
    : proxies_(proxies)
    , httpClient_(client) {}

void ProxyController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/upload", [this](auto& ctx) { handleUpload(ctx); });
    router.addRoute("GET", "/api/expand", [this](auto& ctx) { handleExpand(ctx); });
    router.addRoute("GET", "/api/getItemSkuInfo", [this](auto& ctx) { handleGetItemSkuInfo(ctx); });
    router.addRoute("POST", "/api/loginbyvcode", [this](auto& ctx) { handleLoginByVcode(ctx); });
    router.addRoute("POST", "/api/getListCart", [this](auto& ctx) { handleGetListCart(ctx); });
    router.addRoute("POST", "/api/getUserInfo", [this](auto& ctx) { handleGetUserInfo(ctx); });
    router.addRoute("POST", "/api/getAddOrderData", [this](auto& ctx) { handleGetAddOrderData(ctx); });
    router.addRoute("POST", "/api/proxy", [this](auto& ctx) { handleProxyRequest(ctx); });
    router.addRoute("GET", "/api/proxies", [this](auto& ctx) { handleList(ctx); });
    router.addRoute("POST", "/api/proxies/hydrate", [this](auto& ctx) { handleHydrate(ctx); });
}

void ProxyController::handleUpload(quickgrab::server::RequestContext& ctx) {
    auto boundary = parseBoundary(ctx.request);
    if (!boundary) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing boundary\"}");
        return;
    }

    auto queryParams = parseQueryParameters(ctx.request.target());
    std::unordered_map<std::string, std::string> emptyForm;

    auto cookies = extractMultipartTextField(ctx.request.body(), *boundary, "customCookies");
    auto filePart = extractMultipartFile(ctx.request.body(), *boundary, "file");

    if (!cookies || !filePart) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"invalid multipart payload\"}");
        return;
    }

    bool useProxy = shouldUseProxy(queryParams, emptyForm);
    if (auto flag = extractMultipartTextField(ctx.request.body(), *boundary, "useProxy")) {
        useProxy = parseBoolString(*flag);
    }

    std::string targetUrl = "https://vimg.weidian.com/upload/v3/direct?scope=addorder&fileType=image";
    std::string affinity = resolveAffinityKey(queryParams, emptyForm, targetUrl);
    if (auto aff = extractMultipartTextField(ctx.request.body(), *boundary, "proxyAffinity")) {
        if (!aff->empty()) {
            affinity = trim(*aff);
        }
    }

    std::string newBoundary = randomBoundary();
    std::string payload;

    auto append = [&payload](std::string_view text) {
        payload.append(text.data(), text.size());
    };

    append("--"); append(newBoundary); append("\r\n");
    append("Content-Disposition: form-data; name=\"file\"");
    if (!filePart->filename.empty()) {
        append("; filename=\"");
        append(filePart->filename);
        append("\"");
    }
    append("\r\n");
    if (!filePart->contentType.empty()) {
        append("Content-Type: "); append(filePart->contentType); append("\r\n");
    }
    append("\r\n");
    payload.append(filePart->data);
    append("\r\n");

    append("--"); append(newBoundary); append("\r\nContent-Disposition: form-data; name=\"unadjust\"\r\n\r\nfalse\r\n");
    append("--"); append(newBoundary); append("\r\nContent-Disposition: form-data; name=\"prv\"\r\n\r\nfalse\r\n");
    append("--"); append(newBoundary); append("--\r\n");

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "multipart/form-data; boundary=" + newBoundary},
        {"Cookie", trim(*cookies)},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kMobileUA}
    };

    try {
        auto response = httpClient_.fetch("POST",
                                          targetUrl,
                                          headers,
                                          payload,
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"uploadImage failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}

void ProxyController::handleExpand(quickgrab::server::RequestContext& ctx) {
    auto queryParams = parseQueryParameters(ctx.request.target());
    std::unordered_map<std::string, std::string> emptyForm;
    auto it = queryParams.find("shortUrl");
    if (it == queryParams.end() || it->second.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing shortUrl\"}");
        return;
    }

    bool useProxy = shouldUseProxy(queryParams, emptyForm);
    std::string affinity = resolveAffinityKey(queryParams, emptyForm, it->second);

    std::string finalUrl;
    try {
        auto response = httpClient_.fetch("GET",
                                          it->second,
                                          {},
                                          "",
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          true,
                                          5,
                                          &finalUrl,
                                          useProxy);
        if (response.result_int() >= 400) {
            proxyResponseToContext(ctx, response);
            return;
        }
        sendTextResponse(ctx, boost::beast::http::status::ok, finalUrl);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"expandShortUrl failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}

void ProxyController::handleGetItemSkuInfo(quickgrab::server::RequestContext& ctx) {
    auto queryParams = parseQueryParameters(ctx.request.target());
    auto form = parseFormUrlEncoded(ctx.request.body());

    std::string paramValue;
    if (auto it = queryParams.find("param"); it != queryParams.end()) {
        paramValue = it->second;
    } else if (auto it = form.find("param"); it != form.end()) {
        paramValue = it->second;
    }

    if (paramValue.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing param\"}");
        return;
    }

    std::string encodedParam = urlEncode(paramValue);
    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, encodedParam);

    std::vector<util::HttpClient::Header> headers{
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA},
        {"Accept", "application/json"}
    };

    try {
        auto response = httpClient_.fetch("GET",
                                          "https://thor.weidian.com/detail/getItemSkuInfo/1.0?param=" + encodedParam,
                                          headers,
                                          "",
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"getItemSkuInfo failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}

void ProxyController::handleLoginByVcode(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto queryParams = parseQueryParameters(ctx.request.target());
    auto phone = form.find("phone");
    auto country = form.find("countryCode");
    auto vcode = form.find("vcode");
    if (phone == form.end() || country == form.end() || vcode == form.end()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing parameters\"}");
        return;
    }

    std::string body = "phone=" + urlEncode(phone->second) +
                       "&countryCode=" + urlEncode(country->second) +
                       "&vcode=" + urlEncode(vcode->second);

    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, phone->second);

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}
    };

    try {
        auto response = httpClient_.fetch("POST",
                                          "https://sso.weidian.com/user/loginbyvcode",
                                          headers,
                                          body,
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"loginByVcode failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}\n\nvoid ProxyController::handleGetListCart(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto queryParams = parseQueryParameters(ctx.request.target());

    std::string cookie;
    if (auto it = form.find("cookie"); it != form.end()) {
        cookie = trim(it->second);
    } else if (auto it = queryParams.find("cookie"); it != queryParams.end()) {
        cookie = trim(it->second);
    }

    if (cookie.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing cookie\"}");
        return;
    }

    std::string paramJson = "{\"source\":\"h5\",\"v_seller_id\":\"\",\"tabKey\":\"all\"}";
    std::string body = "param=" + urlEncode(paramJson);

    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, cookie);

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Cookie", cookie},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}
    };

    try {
        auto response = httpClient_.fetch("POST",
                                          "https://thor.weidian.com/vcart/getListCart/3.0",
                                          headers,
                                          body,
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"getListCart failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}\n\nvoid ProxyController::handleGetUserInfo(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto queryParams = parseQueryParameters(ctx.request.target());

    std::string cookie;
    if (auto it = form.find("cookie"); it != form.end()) {
        cookie = trim(it->second);
    } else if (auto it = queryParams.find("cookie"); it != queryParams.end()) {
        cookie = trim(it->second);
    }

    if (cookie.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing cookie\"}");
        return;
    }

    std::string url = "https://thor.weidian.com/udccore/udc.user.getUserInfoById/1.0?param=" + urlEncode("{}");
    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, cookie);

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
        {"Cookie", cookie},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}
    };

    try {
        auto response = httpClient_.fetch("GET",
                                          url,
                                          headers,
                                          "",
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"getUserInfo failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}\n\nvoid ProxyController::handleGetAddOrderData(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto queryParams = parseQueryParameters(ctx.request.target());

    std::string link;
    std::string cookie;

    if (auto it = form.find("link"); it != form.end()) {
        link = trim(it->second);
    }
    if (auto it = form.find("cookie"); it != form.end()) {
        cookie = trim(it->second);
    }

    if (link.empty()) {
        if (auto it = queryParams.find("link"); it != queryParams.end()) {
            link = trim(it->second);
        }
    }
    if (cookie.empty()) {
        if (auto it = queryParams.find("cookie"); it != queryParams.end()) {
            cookie = trim(it->second);
        }
    }

    if (link.empty() || cookie.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing link or cookie\"}");
        return;
    }

    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, link);

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
        {"Cookie", cookie},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}
    };

    try {
        auto response = httpClient_.fetch("GET",
                                          link,
                                          headers,
                                          "",
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        auto dataObject = util::extractDataObject(response.body());
        if (!dataObject) {
            sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"failed to extract data obj\"}");
            return;
        }
        sendJsonResponse(ctx, boost::beast::http::status::ok, boost::json::serialize(*dataObject));
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"getAddOrderData failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}\n\nvoid ProxyController::handleProxyRequest(quickgrab::server::RequestContext& ctx) {
    auto form = parseFormUrlEncoded(ctx.request.body());
    auto queryParams = parseQueryParameters(ctx.request.target());

    auto lookup = [&](const std::string& key) -> std::string {
        if (auto it = form.find(key); it != form.end()) {
            return trim(it->second);
        }
        if (auto it = queryParams.find(key); it != queryParams.end()) {
            return trim(it->second);
        }
        return {};
    };

    std::string targetUrl = lookup("url");
    std::string cookie = lookup("cookie");
    std::string method = lookup("method");
    std::string body = lookup("body");

    if (targetUrl.empty() || cookie.empty()) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request, "{\"error\":\"missing url or cookie\"}");
        return;
    }

    if (method.empty()) {
        method = "GET";
    }

    bool useProxy = shouldUseProxy(queryParams, form);
    std::string affinity = resolveAffinityKey(queryParams, form, targetUrl);

    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
        {"Cookie", cookie},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}
    };

    try {
        auto response = httpClient_.fetch(method,
                                          targetUrl,
                                          headers,
                                          body,
                                          useProxy ? affinity : std::string{},
                                          std::chrono::seconds{30},
                                          false,
                                          5,
                                          nullptr,
                                          useProxy);
        proxyResponseToContext(ctx, response);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"proxyRequest failed: "} + ex.what());
        sendJsonResponse(ctx, boost::beast::http::status::internal_server_error,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}

void ProxyController::handleList(quickgrab::server::RequestContext& ctx) {
    auto proxies = proxies_.listProxies();
    boost::json::array payload;
    for (const auto& proxy : proxies) {
        payload.emplace_back(boost::json::object{
            {"host", proxy.host},
            {"port", proxy.port},
            {"username", proxy.username},
            {"password", proxy.password},
            {"nextAvailable", static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(proxy.nextAvailable.time_since_epoch()).count())},
            {"failureCount", proxy.failureCount}
        });
    }
    sendJsonResponse(ctx, boost::beast::http::status::ok, boost::json::serialize(payload));
}

void ProxyController::handleHydrate(quickgrab::server::RequestContext& ctx) {
    try {
        auto json = boost::json::parse(ctx.request.body());
        if (!json.is_array()) {
            throw std::runtime_error("payload must be array");
        }
        std::vector<proxy::ProxyEndpoint> proxies;
        for (const auto& item : json.as_array()) {
            if (!item.is_object()) {
                continue;
            }
            proxy::ProxyEndpoint endpoint;
            const auto& obj = item.as_object();
            if (auto it = obj.if_contains("host")) endpoint.host = std::string(it->as_string());
            if (auto it = obj.if_contains("port")) endpoint.port = static_cast<std::uint16_t>(it->as_int64());
            if (auto it = obj.if_contains("username")) endpoint.username = it->is_string() ? std::string(it->as_string()) : "";
            if (auto it = obj.if_contains("password")) endpoint.password = it->is_string() ? std::string(it->as_string()) : "";
            endpoint.nextAvailable = std::chrono::steady_clock::now();
            proxies.push_back(std::move(endpoint));
        }
        proxies_.addProxies(std::move(proxies));
        sendJsonResponse(ctx, boost::beast::http::status::ok, "{\"status\":\"ok\"}");
    } catch (const std::exception& ex) {
        sendJsonResponse(ctx, boost::beast::http::status::bad_request,
                         std::string("{\"error\":\"") + ex.what() + "\"}");
    }
}

} // namespace quickgrab::controller
