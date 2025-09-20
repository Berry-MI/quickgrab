#include "quickgrab/controller/AuthController.hpp"
#include "quickgrab/controller/GrabController.hpp"
#include "quickgrab/controller/ProxyController.hpp"
#include "quickgrab/controller/QueryController.hpp"
#include "quickgrab/controller/StatisticsController.hpp"
#include "quickgrab/controller/SubmitController.hpp"
#include "quickgrab/controller/ToolController.hpp"
#include "quickgrab/controller/UserController.hpp"
#include "quickgrab/proxy/ProxyPool.hpp"
#include "quickgrab/repository/DatabaseConfig.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"
#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/server/HttpServer.hpp"
#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/GrabService.hpp"
#include "quickgrab/service/MailService.hpp"
#include "quickgrab/service/ProxyService.hpp"
#include "quickgrab/service/QueryService.hpp"
#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace {
using quickgrab::proxy::ProxyEndpoint;

repository::DatabaseConfig loadDatabaseConfig(const std::filesystem::path& path) {
    repository::DatabaseConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
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
                } catch (const std::exception& ex) {
                    quickgrab::util::log(quickgrab::util::LogLevel::warn,
                                         std::string{"解析数据库配置失败: "} + ex.what());
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
    proxy::ProxyPool proxyPool{std::chrono::seconds{30}};
    util::HttpClient httpClient{io, proxyPool};

    std::filesystem::create_directories("data");
    auto dbConfig = loadDatabaseConfig("data/database.json");
    util::log(util::LogLevel::info,
              "连接 MySQL: " + dbConfig.host + ":" + std::to_string(dbConfig.port) + "/" + dbConfig.database);
    repository::MySqlConnectionPool connectionPool{dbConfig};
    repository::RequestsRepository requests{connectionPool};
    repository::ResultsRepository results{connectionPool};

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

    service::GrabService grabService{io, workerPool, requests, results, httpClient, proxyPool, mailService};
    service::ProxyService proxyService{io, proxyPool};
    service::QueryService queryService{requests, results};

    auto initialProxies = loadProxiesFromFile("data/proxies.json");
    if (!initialProxies.empty()) {
        proxyService.addProxies(std::move(initialProxies));
    }

    auto router = std::make_shared<server::Router>();
    controller::GrabController grabController{grabService};
    grabController.registerRoutes(*router);

    controller::ProxyController proxyController{proxyService, httpClient};
    proxyController.registerRoutes(*router);

    controller::QueryController queryController{queryService};
    queryController.registerRoutes(*router);

    controller::StatisticsController statisticsController;
    statisticsController.registerRoutes(*router);

    controller::SubmitController submitController;
    submitController.registerRoutes(*router);

    controller::ToolController toolController;
    toolController.registerRoutes(*router);

    controller::UserController userController;
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



