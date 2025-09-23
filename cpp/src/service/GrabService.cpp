#include "quickgrab/service/GrabService.hpp"
#include "quickgrab/model/Result.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace quickgrab::service {
namespace {

constexpr std::chrono::milliseconds kProxyProbeTimeout{1500};

bool parseBoolValue(const boost::json::value& value) {
    if (value.is_bool()) {
        return value.as_bool();
    }
    if (value.is_int64()) {
        return value.as_int64() != 0;
    }
    if (value.is_double()) {
        return value.as_double() != 0.0;
    }
    if (value.is_string()) {
        auto str = value.as_string();
        std::string lower(str.c_str(), str.size());
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower == "true" || lower == "1" || lower == "yes";
    }
    return false;
}

bool extensionRequestsProxy(const boost::json::object& extension) {
    static constexpr std::array<std::string_view, 4> keys{"useProxy", "use_proxy", "proxyEnabled", "proxy"};
    for (auto key : keys) {
        if (auto it = extension.if_contains(key.data())) {
            if (parseBoolValue(*it)) {
                return true;
            }
        }
    }
    return false;
}

std::chrono::milliseconds measureProxyLatency(const proxy::ProxyEndpoint& endpoint) {
    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::steady_timer timer(io);
        std::chrono::milliseconds latency = std::chrono::milliseconds::max();
        bool connected = false;

        auto start = std::chrono::steady_clock::now();
        auto endpoints = resolver.resolve(endpoint.host, std::to_string(endpoint.port));

        boost::asio::async_connect(socket, endpoints,
                                   [&](const boost::system::error_code& ec,
                                       const boost::asio::ip::tcp::endpoint&) {
                                       if (!ec) {
                                           connected = true;
                                           latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start);
                                           timer.cancel();
                                       }
                                   });

        timer.expires_after(kProxyProbeTimeout);
        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec) {
                socket.cancel();
            }
        });

        io.run();

        if (connected) {
            boost::system::error_code ec;
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
            return latency;
        }
    } catch (const std::exception&) {
    }
    return kProxyProbeTimeout * 2;
}

} // namespace

GrabService::GrabService(boost::asio::io_context& io,
                         boost::asio::thread_pool& worker,
                         repository::RequestsRepository& requests,
                         repository::ResultsRepository& results,
                         util::HttpClient& client,
                         proxy::ProxyPool& proxies,
                         MailService& mailService)
    : io_(io)
    , worker_(worker)
    , requests_(requests)
    , results_(results)
    , httpClient_(client)
    , proxyPool_(proxies)
    , mailService_(mailService)
    , workflow_(std::make_unique<workflow::GrabWorkflow>(io_, worker_, httpClient_, proxyPool_))
    , adjustedFactor_(10)
    , processingTime_(19)
    , updateTime_(std::chrono::system_clock::now())
    , prestartTime_(std::chrono::system_clock::now())
    , schedulingTime_(computeSchedulingTime()) {}

void GrabService::setProxyConfig(proxy::KdlProxyConfig config) {
    std::lock_guard<std::mutex> lock(proxyMutex_);
    proxyConfig_ = std::move(config);
}

bool GrabService::requestWantsProxy(const boost::json::object& extension) const {
    return extensionRequestsProxy(extension);
}

std::optional<proxy::ProxyEndpoint> GrabService::fetchProxyForRequest(const model::Request& request) {
    std::optional<proxy::KdlProxyConfig> config;
    {
        std::lock_guard<std::mutex> lock(proxyMutex_);
        config = proxyConfig_;
    }
    if (!config) {
        return std::nullopt;
    }

    try {
        auto proxies = proxy::fetchKdlProxies(*config, httpClient_);
        if (proxies.empty()) {
            return std::nullopt;
        }

        for (auto& endpoint : proxies) {
            endpoint.latency = measureProxyLatency(endpoint);
        }

        std::stable_sort(proxies.begin(), proxies.end(), [](const proxy::ProxyEndpoint& lhs,
                                                            const proxy::ProxyEndpoint& rhs) {
            if (lhs.latency == rhs.latency) {
                return lhs.host < rhs.host;
            }
            return lhs.latency < rhs.latency;
        });

        auto best = proxies.front();
        util::log(util::LogLevel::info,
                  "请求 id=" + std::to_string(request.id) +
                      " 分配代理 " + best.host + ':' + std::to_string(best.port) +
                      " (" + std::to_string(best.latency.count()) + "ms)");
        return best;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  std::string{"请求 id="} + std::to_string(request.id) +
                      " 拉取代理失败: " + ex.what());
    }
    return std::nullopt;
}

void GrabService::processPending() {
    boost::asio::post(worker_, [this]() {
        auto pending = requests_.findPending(50);
        boost::asio::post(io_, [this, pending = std::move(pending)]() mutable {
            for (auto& request : pending) {
                executeRequest(std::move(request));
            }
        });
    });
}

std::optional<int> GrabService::handleRequest(const model::Request& request) {
    try {
        return requests_.insert(request);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error,
                  std::string{"插入抢购请求失败: "} + ex.what());
        return std::nullopt;
    }
}

void GrabService::executeRequest(model::Request request) {
    executeGrab(std::move(request));
}

void GrabService::executeGrab(model::Request request) {
    util::log(util::LogLevel::info, "开始处理抢购请求 id=" + std::to_string(request.id));

    const auto threadId = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    try {
        requests_.updateThreadId(request.id, threadId);
        request.threadId = threadId;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  "更新请求线程信息失败 id=" + std::to_string(request.id) + " error=" + ex.what());
    }

    const auto now = std::chrono::system_clock::now();
    const long adjustedLatency = computeAdjustedLatency(request);
    const long processingWindow = computeProcessingTime(request);

    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - updateTime_) > std::chrono::milliseconds(5000)) {
            adjustedFactor_.store(adjustedLatency);
            updateTime_ = now;
        }
        prestartTime_ = now;
        processingTime_ = processingWindow;
    }

    boost::json::object extension;
    if (request.extension.is_object()) {
        extension = request.extension.as_object();
    }

    if (requestWantsProxy(extension)) {
        auto proxy = fetchProxyForRequest(request);
        if (proxy) {
            extension["__proxyHost"] = proxy->host;
            extension["__proxyPort"] = static_cast<std::int64_t>(proxy->port);
            extension["__proxyLatency"] = static_cast<std::int64_t>(proxy->latency.count());
            extension["__proxySource"] = "kdl";
            extension["__proxyAssigned"] = true;
            extension["useProxy"] = true;
            if (!proxy->username.empty()) {
                extension["__proxyUsername"] = proxy->username;
            }
            if (!proxy->password.empty()) {
                extension["__proxyPassword"] = proxy->password;
            }
        } else {
            extension["__proxyAssigned"] = false;
            extension["__proxyError"] = "fetch_failed";
            extension["useProxy"] = false;
            extension["use_proxy"] = false;
            extension["proxy"] = false;
            extension["proxyEnabled"] = false;
        }
    }

    extension["__adjustedFactor"] = adjustedFactor_.load();
    extension["__processingTime"] = processingTime_;
    extension["__schedulingTime"] = schedulingTime_;
    extension["__updatedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    request.extension = extension;

    const auto start = request.startTime;
    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(start - now).count();
    const auto delayHint = request.delay;
    const long waitMillis = std::max<long>(
        0, static_cast<long>(delta) + (delayHint - adjustedFactor_.load() - processingTime_));
    util::log(util::LogLevel::info,
              "请求 id=" + std::to_string(request.id) + " 将在 " + std::to_string(waitMillis) +
                  "ms 后执行 (delay=" + std::to_string(delayHint) +
                  ", latency=" + std::to_string(adjustedFactor_.load()) +
                  ", processing=" + std::to_string(processingTime_) + ")");

    workflow_->run(request, [this, request](const workflow::GrabResult& result) {
        handleResult(request, result);
    });
}

void GrabService::handleResult(const model::Request& request, const workflow::GrabResult& result) {
    model::Result stored;
    stored.requestId = request.id;
    stored.deviceId = request.deviceId;
    stored.buyerId = request.buyerId;
    stored.threadId = request.threadId;
    stored.link = request.link;
    stored.cookies = request.cookies;
    stored.orderInfo = request.orderInfo;
    stored.userInfo = request.userInfo;
    stored.orderTemplate = request.orderTemplate;
    stored.message = request.message;
    stored.idNumber = request.idNumber;
    stored.keyword = request.keyword;
    stored.startTime = request.startTime;
    stored.endTime = request.endTime;
    stored.quantity = request.quantity;
    stored.delay = request.delay;
    stored.frequency = request.frequency;
    stored.type = request.type;
    stored.status = result.statusCode;
    stored.actualEarnings = request.actualEarnings;
    stored.estimatedEarnings = request.estimatedEarnings;
    stored.extension = request.extension;
    stored.createdAt = std::chrono::system_clock::now();
    boost::json::object payload;
    payload["success"] = result.success;
    payload["shouldContinue"] = result.shouldContinue;
    payload["shouldUpdate"] = result.shouldUpdate;
    payload["statusCode"] = result.statusCode;
    payload["message"] = result.message;
    payload["attempts"] = result.attempts;
    if (!result.error.empty()) {
        payload["error"] = result.error;
    }
    if (!result.response.is_null()) {
        payload["response"] = result.response;
    }
    stored.payload = std::move(payload);
    stored.responseMessage = stored.payload;

    if (result.success) {
        util::log(util::LogLevel::info, "抢购完成 id=" + std::to_string(request.id));
        requests_.updateStatus(request.id, 1);
        results_.insertResult(stored);
        mailService_.sendSuccessEmail(request, result);
        try {
            requests_.deleteById(request.id);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn,
                      "删除请求失败 id=" + std::to_string(request.id) + " error=" + ex.what());
        }
        return;
    }

    if (result.shouldContinue || result.shouldUpdate) {
        util::log(util::LogLevel::warn, "抢购请求需继续 id=" + std::to_string(request.id));
        requests_.updateStatus(request.id, 4);
    } else {
        util::log(util::LogLevel::error,
                  "抢购失败 id=" + std::to_string(request.id) + " 原因=" + (!result.message.empty() ? result.message : result.error));
        requests_.updateStatus(request.id, 3);
    }

    results_.insertResult(stored);
    mailService_.sendFailureEmail(request, result);
    if (!result.shouldContinue && !result.shouldUpdate) {
        try {
            requests_.deleteById(request.id);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn,
                      "删除请求失败 id=" + std::to_string(request.id) + " error=" + ex.what());
        }
    }
}

long GrabService::computeAdjustedLatency(const model::Request& request) const {
    auto readLatency = [](const boost::json::value* value) -> std::optional<long> {
        if (!value) {
            return std::nullopt;
        }
        if (value->is_int64()) {
            return static_cast<long>(value->as_int64());
        }
        if (value->is_double()) {
            return static_cast<long>(value->as_double());
        }
        return std::nullopt;
    };

    if (request.extension.is_object()) {
        const auto& ext = request.extension.as_object();
        if (auto latency = readLatency(ext.if_contains("networkDelay"))) {
            return *latency;
        }
        if (auto latency = readLatency(ext.if_contains("adjustedFactor"))) {
            return *latency;
        }
    }
    return adjustedFactor_.load();
}

long GrabService::computeProcessingTime(const model::Request& request) const {
    if (request.extension.is_object()) {
        const auto& ext = request.extension.as_object();
        if (auto* value = ext.if_contains("processingTime")) {
            if (value->is_int64()) {
                return static_cast<long>(value->as_int64());
            }
            if (value->is_double()) {
                return static_cast<long>(value->as_double());
            }
        }
    }
    return processingTime_;
}

long GrabService::computeSchedulingTime() const {
    return 2;
}

} // namespace quickgrab::service
