#include "quickgrab/proxy/KdlProxyClient.hpp"

#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace quickgrab::proxy {
namespace {

std::string_view trimView(std::string_view input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string urlEncode(const std::string& value) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << static_cast<char>(c);
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return oss.str();
}

} // namespace

std::optional<KdlProxyConfig> loadKdlProxyConfig(const std::filesystem::path& path) {
    KdlProxyConfig config;

    if (std::filesystem::exists(path)) {
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            if (!content.empty()) {
                try {
                    auto json = quickgrab::util::parseJson(content);
                    if (json.is_object()) {
                        const auto& obj = json.as_object();
                        if (auto it = obj.if_contains("endpoint"); it && it->is_string()) {
                            config.endpoint = std::string(it->as_string());
                        }
                        if (auto it = obj.if_contains("secretId"); it && it->is_string()) {
                            config.secretId = std::string(it->as_string());
                        }
                        if (auto it = obj.if_contains("signature"); it && it->is_string()) {
                            config.signature = std::string(it->as_string());
                        }
                        if (auto it = obj.if_contains("username"); it && it->is_string()) {
                            config.username = std::string(it->as_string());
                        }
                        if (auto it = obj.if_contains("password"); it && it->is_string()) {
                            config.password = std::string(it->as_string());
                        }
                        auto parseCount = [&config](const boost::json::value* value) {
                            if (value && value->is_int64()) {
                                auto count = value->as_int64();
                                if (count > 0) {
                                    config.batchSize = static_cast<unsigned int>(count);
                                }
                            }
                        };
                        parseCount(obj.if_contains("count"));
                        parseCount(obj.if_contains("num"));
                        parseCount(obj.if_contains("batchSize"));
                        if (auto it = obj.if_contains("refreshMinutes"); it && it->is_int64()) {
                            auto minutes = it->as_int64();
                            if (minutes > 0) {
                                config.refreshInterval = std::chrono::minutes(minutes);
                            }
                        }
                    }
                } catch (const std::exception& ex) {
                    quickgrab::util::log(quickgrab::util::LogLevel::warn,
                                         std::string{"解析 KDL 代理配置失败: "} + ex.what());
                }
            }
        }
    }

    if (const char* value = std::getenv("QUICKGRAB_PROXY_ENDPOINT")) {
        config.endpoint = value;
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_SECRET_ID")) {
        config.secretId = value;
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_SIGNATURE")) {
        config.signature = value;
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_USERNAME")) {
        config.username = value;
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_PASSWORD")) {
        config.password = value;
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_BATCH")) {
        auto count = std::strtoul(value, nullptr, 10);
        if (count > 0) {
            config.batchSize = static_cast<unsigned int>(count);
        }
    }
    if (const char* value = std::getenv("QUICKGRAB_PROXY_REFRESH_MINUTES")) {
        auto minutes = std::strtoul(value, nullptr, 10);
        if (minutes > 0) {
            config.refreshInterval = std::chrono::minutes(minutes);
        }
    }

    if (config.endpoint.empty()) {
        config.endpoint = "https://dps.kdlapi.com/api/getdps/";
    }
    if (config.batchSize == 0) {
        config.batchSize = 1;
    }
    if (config.refreshInterval.count() <= 0) {
        config.refreshInterval = std::chrono::minutes{5};
    }

    if (config.secretId.empty() || config.signature.empty()) {
        return std::nullopt;
    }
    return config;
}

std::vector<ProxyEndpoint> fetchKdlProxies(const KdlProxyConfig& config,
                                           quickgrab::util::HttpClient& httpClient) {
    std::string url = config.endpoint;
    bool hasQuery = url.find('?') != std::string::npos;
    auto appendParam = [&url, &hasQuery](std::string_view key, const std::string& value) {
        url += hasQuery ? '&' : '?';
        hasQuery = true;
        url.append(key.begin(), key.end());
        url.push_back('=');
        url += urlEncode(value);
    };

    appendParam("secret_id", config.secretId);
    appendParam("signature", config.signature);
    appendParam("num", std::to_string(config.batchSize));
    appendParam("format", "text");
    appendParam("sep", "1");

    std::vector<quickgrab::util::HttpClient::Header> headers{
        {"User-Agent",
         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36"},
        {"Accept", "text/plain"}
    };

    auto response = httpClient.fetch("GET",
                                      url,
                                      headers,
                                      "",
                                      std::string{},
                                      std::chrono::seconds{15},
                                      false,
                                      0,
                                      nullptr,
                                      false);

    if (response.result() != boost::beast::http::status::ok) {
        throw std::runtime_error("KDL proxy API returned status " + std::to_string(response.result_int()));
    }

    const std::string& body = response.body();
    std::vector<ProxyEndpoint> proxies;
    proxies.reserve(config.batchSize);
    const auto now = std::chrono::steady_clock::now();

    std::size_t start = 0;
    while (start < body.size()) {
        auto pos = body.find_first_of("\r\n;,|", start);
        auto length = (pos == std::string::npos) ? body.size() - start : pos - start;
        std::string_view chunk(body.data() + start, length);
        start = (pos == std::string::npos) ? body.size() : pos + 1;

        auto trimmed = trimView(chunk);
        if (trimmed.empty()) {
            continue;
        }

        auto colon = trimmed.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }

        auto hostView = trimView(trimmed.substr(0, colon));
        auto portView = trimView(trimmed.substr(colon + 1));
        if (hostView.empty() || portView.empty()) {
            continue;
        }

        unsigned long portValue = 0;
        try {
            portValue = std::stoul(std::string(portView));
        } catch (const std::exception&) {
            quickgrab::util::log(quickgrab::util::LogLevel::warn,
                                 "跳过非法代理条目: " + std::string(trimmed));
            continue;
        }
        if (portValue == 0 || portValue > 65535) {
            quickgrab::util::log(quickgrab::util::LogLevel::warn,
                                 "跳过非法代理端口: " + std::string(trimmed));
            continue;
        }

        ProxyEndpoint endpoint;
        endpoint.host = std::string(hostView);
        endpoint.port = static_cast<std::uint16_t>(portValue);
        endpoint.username = config.username;
        endpoint.password = config.password;
        endpoint.nextAvailable = now;
        proxies.push_back(std::move(endpoint));
    }

    auto trimmedBody = trimView(std::string_view{body});
    if (proxies.empty() && !trimmedBody.empty()) {
        std::string snippet(trimmedBody.substr(0, std::min<std::size_t>(trimmedBody.size(), 120)));
        throw std::runtime_error("KDL proxy API payload unexpected: " + snippet);
    }

    if (proxies.empty()) {
        quickgrab::util::log(quickgrab::util::LogLevel::warn, "KDL 代理接口未返回可用条目");
    } else {
        quickgrab::util::log(quickgrab::util::LogLevel::info,
                             "KDL 代理接口获取 " + std::to_string(proxies.size()) + " 个代理");
    }

    return proxies;
}

} // namespace quickgrab::proxy

