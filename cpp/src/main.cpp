#include "quickgrab/controller/AuthController.hpp"
#include "quickgrab/controller/GrabController.hpp"
#include "quickgrab/controller/ProxyController.hpp"
#include "quickgrab/controller/QueryController.hpp"
#include "quickgrab/controller/StatisticsController.hpp"
#include "quickgrab/controller/SubmitController.hpp"
#include "quickgrab/controller/ToolController.hpp"
#include "quickgrab/controller/UserController.hpp"
#include "quickgrab/proxy/ProxyPool.hpp"
#include "quickgrab/repository/BuyersRepository.hpp"
#include "quickgrab/repository/DatabaseConfig.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"
#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/server/HttpServer.hpp"
#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"
#include "quickgrab/service/GrabService.hpp"
#include "quickgrab/service/MailService.hpp"
#include "quickgrab/service/ProxyService.hpp"
#include "quickgrab/service/QueryService.hpp"
#include "quickgrab/service/StatisticsService.hpp"
#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/http/status.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cctype>

namespace {
    using quickgrab::proxy::ProxyEndpoint;
    using namespace quickgrab;
    repository::DatabaseConfig loadDatabaseConfig(const std::filesystem::path& path) {
        repository::DatabaseConfig config;
        config.host = "127.0.0.1";
        config.port = 33060;
        config.user = "root";
        config.password.clear();
        config.database = "grab_system";
        config.charset = "utf8mb4";
        config.poolSize = std::max(8u, std::thread::hardware_concurrency());

        if (std::filesystem::exists(path)) {
            std::ifstream ifs(path);
            if (ifs.is_open()) {
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                if (!content.empty()) {
                    try {
                        auto json = quickgrab::util::parseJson(content);
                        if (json.is_object()) {
                            auto loaded = repository::loadConfig(json.as_object());
                            if (!loaded.host.empty()) config.host = loaded.host;
                            if (loaded.port != 0) config.port = loaded.port;
                            if (!loaded.user.empty()) config.user = loaded.user;
                            if (!loaded.password.empty()) config.password = loaded.password;
                            if (!loaded.database.empty()) config.database = loaded.database;
                            if (!loaded.charset.empty()) config.charset = loaded.charset;
                            if (loaded.poolSize != 0) config.poolSize = loaded.poolSize;
                        }
                    }
                    catch (const std::exception& ex) {
                        quickgrab::util::log(quickgrab::util::LogLevel::warn,
                            std::string{ "解析数据库配置失败: " } + ex.what());
                    }
                }
            }
        }

        if (const char* value = std::getenv("QUICKGRAB_DB_HOST")) config.host = value;
        if (const char* value = std::getenv("QUICKGRAB_DB_PORT")) config.port = static_cast<std::uint16_t>(std::strtoul(value, nullptr, 10));
        if (const char* value = std::getenv("QUICKGRAB_DB_USER")) config.user = value;
        if (const char* value = std::getenv("QUICKGRAB_DB_PASSWORD")) config.password = value;
        if (const char* value = std::getenv("QUICKGRAB_DB_NAME")) config.database = value;
        if (const char* value = std::getenv("QUICKGRAB_DB_CHARSET")) config.charset = value;
        if (const char* value = std::getenv("QUICKGRAB_DB_POOL")) {
            config.poolSize = std::max(1u, static_cast<unsigned int>(std::strtoul(value, nullptr, 10)));
        }

        return config;
    }

std::vector<ProxyEndpoint> loadProxiesFromFile(const std::filesystem::path& path) {
    std::vector<ProxyEndpoint> proxies;
    if (!std::filesystem::exists(path)) {
        return proxies;
    }
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return proxies;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    try {
        auto json = quickgrab::util::parseJson(content);
        if (!json.is_array()) {
            return proxies;
        }
        for (const auto& item : json.as_array()) {
            if (!item.is_object()) {
                continue;
            }
            ProxyEndpoint endpoint;
            const auto& obj = item.as_object();
            if (auto it = obj.if_contains("host")) endpoint.host = std::string(it->as_string());
            if (auto it = obj.if_contains("port")) endpoint.port = static_cast<std::uint16_t>(it->as_int64());
            if (auto it = obj.if_contains("username")) endpoint.username = it->is_string() ? std::string(it->as_string()) : "";
            if (auto it = obj.if_contains("password")) endpoint.password = it->is_string() ? std::string(it->as_string()) : "";
            endpoint.nextAvailable = std::chrono::steady_clock::now();
            proxies.push_back(std::move(endpoint));
        }
    } catch (const std::exception& ex) {
        quickgrab::util::log(quickgrab::util::LogLevel::warn, std::string{"加载本地代理失败: "} + ex.what());
    }
    return proxies;
}


struct KdlProxyConfig {
    std::string endpoint{"https://dps.kdlapi.com/api/getdps/"};
    std::string secretId;
    std::string signature;
    std::string username;
    std::string password;
    unsigned int batchSize{5};
    std::chrono::minutes refreshInterval{std::chrono::minutes{5}};
};

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
                             "KDL 代理接口刷新成功，获取 " + std::to_string(proxies.size()) + " 个代理");
    }

    return proxies;
}

void startRequestPump(boost::asio::io_context& io, quickgrab::service::GrabService& grabService) {
    auto timer = std::make_shared<boost::asio::steady_timer>(io);
    auto handler = std::make_shared<std::function<void(const boost::system::error_code&)>>();
    *handler = [timer, &grabService, handler](const boost::system::error_code& ec) {
        if (!ec) {
            grabService.processPending();
            timer->expires_after(std::chrono::milliseconds(500));
            timer->async_wait(*handler);
        }
    };
    timer->expires_after(std::chrono::milliseconds(200));
    timer->async_wait(*handler);
}

    void startProxyTick(boost::asio::io_context& io, quickgrab::proxy::ProxyPool& pool) {
        auto timer = std::make_shared<boost::asio::steady_timer>(io);
        auto handler = std::make_shared<std::function<void(const boost::system::error_code&)>>();
        *handler = [timer, &pool, handler](const boost::system::error_code& ec) {
            if (!ec) {
                pool.tick();
                timer->expires_after(std::chrono::seconds(5));
                timer->async_wait(*handler);
            }
            };
        timer->expires_after(std::chrono::seconds(5));
        timer->async_wait(*handler);
    }

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace quickgrab;
    util::initLogging(util::LogLevel::info);

    boost::asio::io_context io;
    boost::asio::thread_pool workerPool(std::max(2u, std::thread::hardware_concurrency()));
    proxy::ProxyPool proxyPool{ std::chrono::seconds{30} };
    util::HttpClient httpClient{ io, proxyPool };

    std::filesystem::create_directories("data");
    auto dbConfig = loadDatabaseConfig("../../data/database.json");
    util::log(util::LogLevel::info,
              "连接 MySQL: " + dbConfig.host + ":" + std::to_string(dbConfig.port) + "/" + dbConfig.database);
    repository::MySqlConnectionPool connectionPool{dbConfig};
    repository::RequestsRepository requests{connectionPool};
    repository::ResultsRepository results{connectionPool};
    repository::BuyersRepository buyers{connectionPool};

    service::MailService::Config mailConfig;
    if (const char* from = std::getenv("QUICKGRAB_MAIL_FROM")) {
        mailConfig.fromEmail = from;
    }
    if (const char* sender = std::getenv("QUICKGRAB_MAIL_SENDER")) {
        mailConfig.senderName = sender;
    }
    if (const char* spool = std::getenv("QUICKGRAB_MAIL_OUTBOX")) {
        mailConfig.spoolDirectory = spool;
    }

    service::MailService mailService{std::move(mailConfig)};

    service::AuthService authService{buyers};
    service::GrabService grabService{io, workerPool, requests, results, httpClient, proxyPool, mailService};
    service::ProxyService proxyService{io, proxyPool};
    service::QueryService queryService{requests, results, buyers};
    service::StatisticsService statisticsService{results, buyers};

    auto initialProxies = loadProxiesFromFile("../../data/proxies.json");
    if (!initialProxies.empty()) {
        proxyService.addProxies(std::move(initialProxies));
    }

    if (auto kdlConfig = loadKdlProxyConfig("data/kdlproxy.json")) {
        util::log(util::LogLevel::info,
                  "启用 KDL 代理池刷新，每 " +
                      std::to_string(kdlConfig->refreshInterval.count()) +
                      " 分钟拉取 " + std::to_string(kdlConfig->batchSize) + " 个代理");
        proxyService.scheduleRefresh(
            [&httpClient, config = *kdlConfig, &proxyService]() {
                try {
                    return fetchKdlProxies(config, httpClient);
                } catch (const std::exception& ex) {
                    util::log(util::LogLevel::warn, std::string{"刷新 KDL 代理失败: "} + ex.what());
                    return proxyService.listProxies();
                }
            },
            kdlConfig->refreshInterval);
    }

    auto router = std::make_shared<server::Router>();
    controller::AuthController authController{authService};
    authController.registerRoutes(*router);

    controller::GrabController grabController{grabService};
    grabController.registerRoutes(*router);

    controller::ProxyController proxyController{ proxyService, httpClient };
    proxyController.registerRoutes(*router);

    controller::QueryController queryController{ queryService };
    queryController.registerRoutes(*router);

    controller::StatisticsController statisticsController{statisticsService};
    statisticsController.registerRoutes(*router);

    controller::SubmitController submitController{grabService};
    submitController.registerRoutes(*router);

    controller::ToolController toolController;
    toolController.registerRoutes(*router);

    controller::UserController userController{authService};
    userController.registerRoutes(*router);

    auto server = std::make_shared<server::HttpServer>(io, router, "0.0.0.0", 8080);
    server->start();

    startRequestPump(io, grabService);
    startProxyTick(io, proxyPool);

    unsigned int ioThreadsCount = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> ioThreads;
    if (ioThreadsCount > 1) {
        ioThreads.reserve(ioThreadsCount - 1);
        for (unsigned int i = 0; i < ioThreadsCount - 1; ++i) {
            ioThreads.emplace_back([&io]() { io.run(); });
        }
    }

    util::log(util::LogLevel::info, "QuickGrab C++ server listening on 0.0.0.0:8080");
    io.run();

    for (auto& thread : ioThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    workerPool.join();
    return 0;
}



