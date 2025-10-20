#include "quickgrab/workflow/GrabWorkflow.hpp"
#include "quickgrab/util/CommonUtil.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/WeidianParser.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/asio/post.hpp>
#include <iostream>

namespace quickgrab::workflow {
namespace {
constexpr int kMaxRetries = 3;
constexpr char kUserAgent[] = "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25";
constexpr char kReferer[] = "https://android.weidian.com/";
constexpr char kDesktopUA[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36 Edg/108.0.1462.76";

constexpr std::array<std::string_view, 11> kRetryKeywords{
    "请稍后再试",
    "拥挤",
    "重试",
    "稍后",
    "人潮拥挤",
    "商品尚未开售",
    "开小差",
    "系统开小差",
    "系统开小差了",
    "啊哦~ 人潮拥挤，请稍后重试~",
    "请升级到最新版本后重试"};

constexpr std::array<std::string_view, 12> kUpdateKeywords{
    "确认",
    "地址",
    "自提",
    "应付总额有变动，请再次确认",
    "商品信息变更，请重新确认",
    "模板需要收货地址，请联系商家",
    "店铺信息不能为空",
    "购买的商品超过限购数",
    "请先填写收货人地址",
    "当前下单商品仅支持到店自提，请重新选择收货方式",
    "系统开小差，请稍后重试",
    "自提点地址不能为空"};

std::string randomDomain(const boost::json::object& extension) {
    if (auto it = extension.if_contains("domains")) {
        if (it->is_array() && !it->as_array().empty()) {
            static thread_local std::mt19937 rng{std::random_device{}()};
            const auto& arr = it->as_array();
            std::uniform_int_distribution<std::size_t> dist(0, arr.size() - 1);
            return arr.at(dist(rng)).as_string().c_str();
        }
    }
    return "thor.weidian.com";
}

template <std::size_t N>
bool containsKeyword(std::string_view message, const std::array<std::string_view, N>& keywords) {
    for (auto keyword : keywords) {
        if (message.find(keyword) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

long computeDelay(const GrabContext& ctx) {
    const auto now = std::chrono::system_clock::now();
    const auto start = ctx.request.startTime;
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(start - now).count();
    const auto needDelay = ctx.request.delay;
    long delay = static_cast<long>(needDelay /*- ctx.adjustedFactor - ctx.processingTime*/);
    long advanceMs = 100; //提前启动
    long timeRemaining = std::max<long>(0, static_cast<long>(delta) + delay - advanceMs);
    return timeRemaining;
}

boost::json::object parseExtension(const model::Request& request) {
    boost::json::object ext;
    if (request.extension.is_object()) {
        ext = request.extension.as_object();
    }
    return ext;
}

bool tryGetBool(const boost::json::object& obj, const char* key, bool def) {
    if (auto it = obj.if_contains(key); it && it->is_bool()) {
        return it->as_bool();
    }
    return def;
}

bool parseBool(const boost::json::value& value) {
    if (value.is_bool()) {
        return value.as_bool();
    }
    if (value.is_int64()) {
        return value.as_int64() != 0;
    }
    if (value.is_string()) {
        auto str = value.as_string();
        std::string lower(str.c_str(), str.size());
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower == "true" || lower == "1";
    }
    return false;
}

std::optional<std::string> parseString(const boost::json::value& value) {
    if (value.is_string()) {
        auto str = value.as_string();
        return std::string(str.c_str(), str.size());
    }
    if (value.is_int64()) {
        return std::to_string(value.as_int64());
    }
    if (value.is_double()) {
        return std::to_string(value.as_double());
    }
    return std::nullopt;
}

bool shouldUseProxy(const boost::json::object& extension) {
    static constexpr std::array<std::string_view, 4> keys{"useProxy", "use_proxy", "proxyEnabled", "proxy"};
    for (auto key : keys) {
        if (auto it = extension.if_contains(key.data())) {
            if (parseBool(*it)) {
                return true;
            }
        }
    }
    return false;
}

std::string resolveAffinity(const boost::json::object& extension) {
    static constexpr std::array<std::string_view, 3> keys{"proxyAffinity", "proxyKey", "proxy_key"};
    for (auto key : keys) {
        if (auto it = extension.if_contains(key.data())) {
            if (auto value = parseString(*it)) {
                if (!value->empty()) {
                    return *value;
                }
            }
        }
    }
    return {};
}

std::string toQuery(const std::string& payload) {
    std::string encoded;
    encoded.reserve(payload.size() * 2);
    static constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : payload) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return "param=" + encoded;
}
}

GrabWorkflow::GrabWorkflow(boost::asio::io_context& io,
                           boost::asio::thread_pool& worker,
                           util::HttpClient& httpClient,
                           proxy::ProxyPool& proxyPool)
    : io_(io)
    , worker_(worker)
    , httpClient_(httpClient)
    , proxyPool_(proxyPool) {}

void GrabWorkflow::run(const model::Request& request,
                       std::function<void(const GrabResult&)> onFinished) {
    GrabContext ctx;
    prepareContext(request, ctx);
    const bool pickMode = (request.type == 3) || ctx.autoPick;
    if (pickMode) {
        schedulePick(std::move(ctx), std::move(onFinished));
    } else {
        scheduleExecution(std::move(ctx), std::move(onFinished));
    }
}

void GrabWorkflow::prepareContext(const model::Request& request, GrabContext& ctx) {
    ctx.request = request;
    ctx.extension = parseExtension(request);
    ctx.domain = randomDomain(ctx.extension);
    ctx.start = std::chrono::steady_clock::now();
    ctx.quickMode = tryGetBool(ctx.extension, "quickMode", false);
    ctx.steadyOrder = tryGetBool(ctx.extension, "steadyOrder", false);
    ctx.autoPick = tryGetBool(ctx.extension, "autoPick", false);
    if (auto metric = ctx.extension.if_contains("__adjustedFactor")) {
        if (metric->is_int64()) {
            ctx.adjustedFactor = static_cast<long>(metric->as_int64());
        } else if (metric->is_double()) {
            ctx.adjustedFactor = static_cast<long>(metric->as_double());
        }
    } else if (auto metric = ctx.extension.if_contains("adjustedFactor")) {
        if (metric->is_int64()) {
            ctx.adjustedFactor = static_cast<long>(metric->as_int64());
        }
    }
    if (auto metric = ctx.extension.if_contains("__processingTime")) {
        if (metric->is_int64()) {
            ctx.processingTime = static_cast<long>(metric->as_int64());
        } else if (metric->is_double()) {
            ctx.processingTime = static_cast<long>(metric->as_double());
        }
    } else if (auto metric = ctx.extension.if_contains("processingTime")) {
        if (metric->is_int64()) {
            ctx.processingTime = static_cast<long>(metric->as_int64());
        }
    }

    ctx.useProxy = shouldUseProxy(ctx.extension);
    ctx.proxyAffinity = resolveAffinity(ctx.extension);
    if (ctx.extension.if_contains("__proxyAssigned") &&
        ctx.extension.at("__proxyAssigned").is_bool() &&
        !ctx.extension.at("__proxyAssigned").as_bool()) {
        ctx.useProxy = false;
    }
    if (auto hostVal = ctx.extension.if_contains("__proxyHost")) {
        if (hostVal->is_string()) {
            proxy::ProxyEndpoint endpoint;
            endpoint.host = std::string(hostVal->as_string());
            if (auto portVal = ctx.extension.if_contains("__proxyPort")) {
                if (portVal->is_int64()) {
                    endpoint.port = static_cast<std::uint16_t>(portVal->as_int64());
                } else if (portVal->is_string()) {
                    try {
                        endpoint.port = static_cast<std::uint16_t>(std::stoi(std::string(portVal->as_string())));
                    } catch (const std::exception&) {
                        endpoint.port = 0;
                    }
                }
            }
            if (auto userVal = ctx.extension.if_contains("__proxyUsername")) {
                if (userVal->is_string()) {
                    endpoint.username = std::string(userVal->as_string());
                }
            }
            if (auto passVal = ctx.extension.if_contains("__proxyPassword")) {
                if (passVal->is_string()) {
                    endpoint.password = std::string(passVal->as_string());
                }
            }
            if (auto latencyVal = ctx.extension.if_contains("__proxyLatency")) {
                if (latencyVal->is_int64()) {
                    endpoint.latency = std::chrono::milliseconds(latencyVal->as_int64());
                }
            }
            endpoint.nextAvailable = std::chrono::steady_clock::now();
            if (endpoint.port != 0 && !endpoint.host.empty()) {
                ctx.assignedProxy = endpoint;
            }
        }
    }
    if (ctx.useProxy && !ctx.assignedProxy) {
        if (ctx.proxyAffinity.empty()) {
            ctx.proxyAffinity = ctx.request.threadId;
        }
        if (ctx.proxyAffinity.empty()) {
            ctx.useProxy = false;
        }
    }
}

GrabResult GrabWorkflow::createOrder(const GrabContext& ctx, const boost::json::object& payload) {
    GrabResult result;
    auto requestBody = quickgrab::util::stringifyJson(payload);

    auto req = buildPost("https://" + ctx.domain + "/vbuy/CreateOrder/1.0", ctx, toQuery(requestBody));
    bool useProxy = ctx.useProxy;
    std::optional<proxy::ProxyEndpoint> overrideProxy = ctx.assignedProxy;
    const std::string& affinity = ctx.proxyAffinity.empty() ? ctx.request.threadId : ctx.proxyAffinity;

    try {
        auto response = httpClient_.fetch(req,
            affinity,
            std::chrono::seconds{ 30 },
            useProxy,
            overrideProxy ? &*overrideProxy : nullptr);
        result.statusCode = static_cast<int>(response.result());

        auto json = quickgrab::util::parseJson(response.body());
        result.response = json;
        result.attempts = 1;

        if (json.is_object()) {
            const auto& obj = json.as_object();
            if (auto* status = obj.if_contains("status"); status && status->is_object()) {
                const auto& statusObj = status->as_object();
                if (auto* description = statusObj.if_contains("description"); description && description->is_string()) {
                    result.description = std::string(description->as_string());
                }
                if (auto* message = statusObj.if_contains("message"); message && message->is_string()) {
                    result.message = std::string(message->as_string());
                }
                if (auto* code = statusObj.if_contains("code"); code && code->is_int64()) {
                    result.statusCode = static_cast<int>(code->as_int64());
                }
            }

            bool success = false;
            if (auto* isSuccess = obj.if_contains("isSuccess"); isSuccess && isSuccess->is_int64()) {
                success = isSuccess->as_int64() == 1;
            }
            if (!success) {
                if (auto* status = obj.if_contains("status"); status && status->is_object()) {
                    const auto& statusObj = status->as_object();
                    if (auto* code = statusObj.if_contains("code"); code && code->is_int64()) {
                        success = code->as_int64() == 0;
                    }
                }
            }

            result.success = success;
            if (result.success) {
                result.shouldContinue = false;
                result.shouldUpdate = false;
                return result;
            }

            bool updateHint = containsKeyword(result.message, kUpdateKeywords) ||
                (obj.if_contains("isUpdate") && obj.at("isUpdate").is_bool() && obj.at("isUpdate").as_bool());
            bool retryHint = containsKeyword(result.message, kRetryKeywords) ||
                (obj.if_contains("isContinue") && obj.at("isContinue").is_bool() && obj.at("isContinue").as_bool());

            result.shouldUpdate = updateHint;
            result.shouldContinue = retryHint && !updateHint;
        }
        else {
            result.message = "未知响应";
            result.shouldContinue = false;
        }

    }
    catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{ "CreateOrder failed: " } + ex.what());
        result.success = false;
        result.error = ex.what();
        result.message.clear();
    }

    return result;
}


GrabResult GrabWorkflow::reConfirmOrder(const GrabContext& ctx, const boost::json::object& payload) {
    GrabResult result;
    auto requestBody = quickgrab::util::stringifyJson(payload);
    auto req = buildPost("https://" + ctx.domain + "/vbuy/ReConfirmOrder/1.0", ctx, toQuery(requestBody));
    bool useProxy = ctx.useProxy;
    std::optional<proxy::ProxyEndpoint> overrideProxy = ctx.assignedProxy;

    std::string lastError;
    auto waitWithJitter = [](int attemptIndex) {
        long baseDelay = static_cast<long>(std::pow(2.0, attemptIndex) * 80);
        baseDelay = std::max<long>(50, baseDelay);
        long jitterRange = baseDelay / 5;
        if (jitterRange > 0) {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_int_distribution<long> dist(-jitterRange, jitterRange);
            baseDelay += dist(rng);
        }
        return std::chrono::milliseconds(baseDelay);
    };

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        const std::string& affinity = ctx.proxyAffinity.empty() ? ctx.request.threadId : ctx.proxyAffinity;
        auto scheduleRetry = [&](std::string_view errorMessage) {
            if (attempt == kMaxRetries) {
                if (!errorMessage.empty() && result.error.empty()) {
                    result.error = std::string(errorMessage);
                }
                return false;
            }
            auto waitTime = waitWithJitter(attempt + 1);
            std::this_thread::sleep_for(waitTime);
            return true;
        };

        try {
            auto response = httpClient_.fetch(req,
                                             affinity,
                                             std::chrono::seconds{20},
                                             useProxy,
                                             overrideProxy ? &*overrideProxy : nullptr);
            result.statusCode = static_cast<int>(response.result());
            auto json = quickgrab::util::parseJson(response.body());
            result.attempts = attempt + 1;

            if (!json.is_object()) {
                lastError = "ReConfirmOrder 响应不是 JSON 对象";
                util::log(util::LogLevel::warn, lastError);
                if (!scheduleRetry(lastError)) {
                    break;
                }
                continue;
            }

            const auto& obj = json.as_object();
            result.response = json;
            bool success = false;
            bool hasIndicator = false;

            if (auto* isSuccess = obj.if_contains("isSuccess")) {
                if (isSuccess->is_int64()) {
                    success = isSuccess->as_int64() == 1;
                    hasIndicator = true;
                } else if (isSuccess->is_bool()) {
                    success = isSuccess->as_bool();
                    hasIndicator = true;
                }
            }

            if (auto* status = obj.if_contains("status"); status && status->is_object()) {
                const auto& statusObj = status->as_object();
                if (auto* description = statusObj.if_contains("description"); description && description->is_string()) {
                    result.description = std::string(description->as_string());
                }
                if (auto* message = statusObj.if_contains("message"); message && message->is_string()) {
                    result.message = std::string(message->as_string());
                }
                if (auto* code = statusObj.if_contains("code"); code && code->is_int64()) {
                    result.statusCode = static_cast<int>(code->as_int64());
                    success = success || code->as_int64() == 0;
                    hasIndicator = true;
                }
            }

            if (auto* message = obj.if_contains("message"); message && message->is_string() && result.message.empty()) {
                result.message = std::string(message->as_string());
            }

            auto* resultValue = obj.if_contains("result");
            bool treatAsSuccess = success;
            if (!hasIndicator && resultValue) {
                treatAsSuccess = true;
            }

            if (treatAsSuccess && resultValue) {
                result.response = *resultValue;
                result.success = true;
                result.shouldContinue = false;
                result.shouldUpdate = false;
                return result;
            }

            lastError = !result.message.empty() ? result.message : "ReConfirmOrder 响应缺少成功结果";
            util::log(util::LogLevel::warn,
                      "ReConfirmOrder attempt " + std::to_string(attempt + 1) + " failed: " + lastError);
            if (!scheduleRetry(lastError)) {
                break;
            }
        } catch (const util::ProxyError& ex) {
            util::log(util::LogLevel::warn, std::string{"ReConfirmOrder attempt failed: "} + ex.what());
            bool retried = false;
            if (overrideProxy) {
                util::log(util::LogLevel::info,
                          std::string("ReConfirmOrder proxy ") + overrideProxy->host + ":" +
                              std::to_string(overrideProxy->port) + " failed with status " +
                              std::to_string(ex.status()) + ", retrying with proxy pool or direct connection");
                overrideProxy.reset();
                retried = true;
            } else if (useProxy) {
                util::log(util::LogLevel::info,
                          std::string("ReConfirmOrder proxy request failed with status ") +
                              std::to_string(ex.status()) + ", retrying without proxy");
                useProxy = false;
                retried = true;
            }
            if (retried) {
                continue;
            }
            lastError = ex.what();
            if (!scheduleRetry(ex.what())) {
                result.error = ex.what();
                break;
            }
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"ReConfirmOrder attempt failed: "} + ex.what());
            lastError = ex.what();
            if (!scheduleRetry(ex.what())) {
                result.error = ex.what();
                break;
            }
        }
    }
    result.success = false;
    if (result.error.empty()) {
        result.error = lastError.empty() ? "ReConfirmOrder exhausted retries" : lastError;
    }
    return result;
}

void GrabWorkflow::scheduleExecution(
    GrabContext ctx,
    std::function<void(const GrabResult&)> onFinished
) {
    refreshOrderParameters(ctx);  //先准备一次请求参数，让第一次请求可以直接调用
    const auto delay_ms = computeDelay(ctx); 
    const auto target_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);

    util::log(
        util::LogLevel::info,
        "请求ID=" + std::to_string(ctx.request.id) +
        (ctx.quickMode ? " [快速模式]" : "") +
        (ctx.steadyOrder ? " [稳定抢购]" : "") +
        (ctx.autoPick ? " [自动选取]" : "") +
        " 将在 " + std::to_string(delay_ms) + "ms 后开始抢购"
    );



    auto timer = std::make_shared<boost::asio::steady_timer>(worker_.get_executor());
    timer->expires_at(target_tp);

    timer->async_wait(
        [this, ctx = std::move(ctx), onFinished = std::move(onFinished), timer]
        (const boost::system::error_code& ec) mutable {
            if (ec) {
                GrabResult cancelled;
                cancelled.success = false;
                cancelled.statusCode = 499;
                cancelled.message = ec.message();
                boost::asio::post(
                    io_,
                    [onFinished = std::move(onFinished), cancelled]() mutable {
                        onFinished(cancelled);
                    }
                );
                return;
            }

            // 由于定时器绑定到了 worker_ 的执行器，抢购流程从延时等待开始便已经在工作线程
            // 上排队执行。这样可以确保单个请求在 worker_ 池中始终只占用一个线程，避免了
            // 先由 io_context 线程触发再切换到 worker_ 造成的双重占用问题。

            // 首次下单：固定 thor
            GrabContext mainCtx = ctx;
            mainCtx.domain = "thor.weidian.com";

            auto result = createOrder(mainCtx, ctx.request.orderParameters.as_object());
            if (result.success) {
                result.statusCode = 1;
            }
            int attemptCount = 1;
            // ==== 指数退避参数（仅用于普通重试分支）====
            std::chrono::milliseconds baseDelay{ 120 };    // 初始等待
            double backoffFactor = 2.0;                  // 指数倍数
            std::chrono::milliseconds maxDelay{ 900 };     // 最大等待
            double jitterRatio = 0.10;                   // 抖动比例 ±10%
            auto currentDelay = baseDelay;
            int consecutivePlainRetries = 0;

            while (result.shouldContinue && attemptCount < 20) {
                if (result.shouldUpdate) {

                    // std::this_thread::sleep_for(std::chrono::milliseconds(300));

                    const auto attemptStart = std::chrono::steady_clock::now();

                    // 先 reConfirm，再视结果刷新参数
                    auto confirm = reConfirmOrder(ctx, ctx.request.orderParameters.as_object());
                    if (confirm.success && confirm.response.is_object()) {
                        refreshOrderParameters(ctx);
                    }

                    // 稳定节流：确保“确认→下单”至少 1s
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - attemptStart);
                    if (elapsed.count() < 1000) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000 - elapsed.count()));
                    }

                    // 更新域名（容灾/分流）
                    GrabContext updatedCtx = ctx;
                    updatedCtx.domain = randomDomain(ctx.extension);

                    result = createOrder(updatedCtx, ctx.request.orderParameters.as_object());
                    if (!result.success) result.statusCode = 2;

                }
                else {
                    // —— 普通重试分支：指数退避 + 抖动 ——
                    // 计算带抖动的等待时间：currentDelay * (1 ± jitter)
                    auto delayMs = currentDelay.count();
                    auto jitterSpan = static_cast<long>(delayMs * jitterRatio);
                    if (jitterSpan > 0) {
                        // [-jitterSpan, +jitterSpan]
                        static thread_local std::mt19937 rng{ std::random_device{}() };
                        std::uniform_int_distribution<long> dist(-jitterSpan, jitterSpan);
                        delayMs += dist(rng);
                        if (delayMs < 0) delayMs = 0;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

                    result = createOrder(ctx, ctx.request.orderParameters.as_object());
                    if (!result.success) result.statusCode = 2;

                    // 指数退避推进（仅在普通重试连续时推进；若 next 次进入更新分支会被重置）
                    ++consecutivePlainRetries;
                    if (result.shouldContinue && !result.shouldUpdate) {
                        // 还在普通重试轨道 → 放大 currentDelay
                        auto next = std::chrono::milliseconds(
                            static_cast<long>(currentDelay.count() * backoffFactor));
                        if (next > maxDelay) next = maxDelay;
                        currentDelay = next;
                    }
                    else {
                        // 出现更新/成功/终止 → 重置退避
                        consecutivePlainRetries = 0;
                        currentDelay = baseDelay;
                    }

                }

                ++attemptCount;
            }

            // 补充统计信息
            if (result.response.is_object()) {
                auto& responseObj = result.response.as_object();
                responseObj["count"] = attemptCount;
                if (attemptCount >= 10 && responseObj.if_contains("isContinue")) {
                    responseObj["isContinue"] = false;
                }
            }
            if (!result.success) result.statusCode = 3; // 最终失败再标
            result.attempts = attemptCount;

            // 切回 io_ 执行回调
            boost::asio::post(
                io_,
                [onFinished = std::move(onFinished), result = std::move(result)]() mutable {
                    onFinished(result);
                }
            );
        }
    );
}

void GrabWorkflow::schedulePick(
    GrabContext ctx,
    std::function<void(const GrabResult&)> onFinished
) {
    util::log(
        util::LogLevel::info,
        "请求ID=" + std::to_string(ctx.request.id) +
            (ctx.quickMode ? " [快速模式]" : "") +
            " [捡漏] 即刻开始执行"
    );

    refreshOrderParameters(ctx);

    auto timer = std::make_shared<boost::asio::steady_timer>(worker_.get_executor());
    timer->expires_after(std::chrono::milliseconds(0));

    timer->async_wait(
        [this, ctx = std::move(ctx), onFinished = std::move(onFinished), timer]
        (const boost::system::error_code& ec) mutable {
            if (ec) {
                GrabResult cancelled;
                cancelled.success = false;
                //cancelled.statusCode = 499;
                cancelled.statusCode = 3;
                cancelled.message = ec.message();
                boost::asio::post(
                    io_,
                    [onFinished = std::move(onFinished), cancelled]() mutable {
                        onFinished(cancelled);
                    }
                );
                return;
            }

            GrabResult finalResult;
            finalResult.success = false;
            finalResult.shouldContinue = false;
            finalResult.shouldUpdate = false;
            finalResult.statusCode = 400;

            do {
                auto* paramsObj = ctx.request.orderParameters.if_object();
                if (!paramsObj && !ctx.request.orderParametersRaw.empty()) {
                    try {
                        auto parsed = quickgrab::util::parseJson(ctx.request.orderParametersRaw);
                        if (parsed.is_object()) {
                            ctx.request.orderParameters = parsed;
                            paramsObj = ctx.request.orderParameters.if_object();
                        }
                    } catch (const std::exception& ex) {
                        util::log(util::LogLevel::warn,
                                  "请求ID=" + std::to_string(ctx.request.id) +
                                      " 解析订单参数失败: " + ex.what());
                    }
                }

                if (!paramsObj) {
                    finalResult.message = "订单参数缺失";
                    break;
                }

                const boost::json::array* shopList = nullptr;
                if (auto* node = paramsObj->if_contains("shop_list"); node && node->is_array()) {
                    shopList = &node->as_array();
                }
                if (!shopList || shopList->empty() || !(*shopList)[0].is_object()) {
                    finalResult.message = "订单参数缺少商品信息";
                    break;
                }

                const auto& firstShop = (*shopList)[0].as_object();
                const boost::json::array* itemList = nullptr;
                if (auto* node = firstShop.if_contains("item_list"); node && node->is_array()) {
                    itemList = &node->as_array();
                }
                if (!itemList || itemList->empty() || !(*itemList)[0].is_object()) {
                    finalResult.message = "订单参数缺少商品信息";
                    break;
                }

                const auto& firstItem = (*itemList)[0].as_object();
                std::optional<std::string> itemId;
                if (auto* node = firstItem.if_contains("item_id")) {
                    itemId = parseString(*node);
                }
                std::optional<std::string> skuId;
                if (auto* node = firstItem.if_contains("item_sku_id")) {
                    skuId = parseString(*node);
                }

                if (!itemId) {
                    finalResult.message = "订单参数缺少商品编号";
                    break;
                }

                std::string skuIdValue = skuId.value_or("0");
                int quantity = 1;
                if (auto* node = firstItem.if_contains("quantity")) {
                    if (node->is_int64()) {
                        quantity = static_cast<int>(node->as_int64());
                    } else if (node->is_double()) {
                        quantity = static_cast<int>(std::lround(node->as_double()));
                    } else if (node->is_string()) {
                        try {
                            quantity = std::stoi(std::string(node->as_string().c_str()));
                        } catch (const std::exception&) {
                        }
                    }
                }
                if (quantity < 1) {
                    quantity = 1;
                }

                auto endTime = ctx.request.endTime;
                const auto now = std::chrono::system_clock::now();
                if (endTime <= now) {
                    endTime = now + std::chrono::minutes(5);
                }

                int frequency = ctx.request.frequency;
                frequency = std::max(frequency - 150, 100);
                util::log(util::LogLevel::info,
                          "请求ID=" + std::to_string(ctx.request.id) + " 检查商品库存");

                bool useProxy = ctx.useProxy;
                std::optional<proxy::ProxyEndpoint> overrideProxy = ctx.assignedProxy;
                const std::string affinity = ctx.proxyAffinity.empty() ? ctx.request.threadId : ctx.proxyAffinity;

                const std::vector<util::HttpClient::Header> commonHeaders{
                    {"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
                    {"Referer", "https://weidian.com/"},
                    {"User-Agent", kDesktopUA},
                };

                auto fetchJson = [&](const std::string& url,
                                     std::chrono::seconds timeout,
                                     std::string_view action) -> std::optional<boost::json::value> {
                    for (int attempt = 0; attempt < 2; ++attempt) {
                        try {
                            auto response = httpClient_.fetch("GET",
                                                              url,
                                                              commonHeaders,
                                                              "",
                                                              affinity,
                                                              timeout,
                                                              true,
                                                              5,
                                                              nullptr,
                                                              useProxy,
                                                              overrideProxy ? &*overrideProxy : nullptr);
                            return quickgrab::util::parseJson(response.body());
                        } catch (const util::ProxyError& ex) {
                            util::log(util::LogLevel::warn,
                                      "请求ID=" + std::to_string(ctx.request.id) + " " + std::string(action) +
                                          " 失败: " + ex.what());
                            bool retried = false;
                            if (overrideProxy) {
                                util::log(util::LogLevel::info,
                                          "请求ID=" + std::to_string(ctx.request.id) + " 指定代理 " + overrideProxy->host +
                                              ":" + std::to_string(overrideProxy->port) + " 失效，将尝试代理池或直连");
                                overrideProxy.reset();
                                retried = true;
                            } else if (useProxy) {
                                util::log(util::LogLevel::info,
                                          "请求ID=" + std::to_string(ctx.request.id) +
                                              " 代理请求失败，将改为直连重试");
                                useProxy = false;
                                retried = true;
                            }
                            if (!retried) {
                                return std::nullopt;
                            }
                        } catch (const std::exception& ex) {
                            util::log(util::LogLevel::warn,
                                      "请求ID=" + std::to_string(ctx.request.id) + " " + std::string(action) +
                                          " 异常: " + ex.what());
                            return std::nullopt;
                        }
                    }
                    return std::nullopt;
                };

                auto buildInventoryUrl = [&](const std::string& domain) {
                    boost::json::object payload;
                    payload["itemId"] = *itemId;
                    payload["source"] = "h5";
                    payload["skuId"] = skuIdValue;
                    payload["count"] = quantity;
                    auto jsonPayload = quickgrab::util::stringifyJson(payload);
                    return "https://" + domain + "/vcart/addCart/2.0?" + toQuery(jsonPayload);
                };

                auto buildSkuInfoUrl = [&](const std::string& domain) {
                    boost::json::object payload;
                    payload["itemId"] = *itemId;
                    auto jsonPayload = quickgrab::util::stringifyJson(payload);
                    return "https://" + domain + "/detailmjb/getItemSkuInfo/1.0?" + toQuery(jsonPayload);
                };

                GrabContext createCtx = ctx;
                createCtx.domain = randomDomain(ctx.extension);

                int count = 0;
                bool virtualItem = false;
                std::optional<GrabResult> pickResult;

                while (std::chrono::system_clock::now() < endTime) {
                    auto inventoryUrl = buildInventoryUrl(randomDomain(ctx.extension));
                    auto inventory = fetchJson(inventoryUrl, std::chrono::seconds{20}, "获取商品库存");
                    if (inventory && inventory->is_object()) {
                        const auto& invObj = inventory->as_object();
                        int statusCode = -1;
                        if (auto* status = invObj.if_contains("status"); status && status->is_object()) {
                            const auto& statusObj = status->as_object();
                            if (auto* code = statusObj.if_contains("code"); code && code->is_int64()) {
                                statusCode = static_cast<int>(code->as_int64());
                            }
                        }

                        if (statusCode == 12) {
                            util::log(util::LogLevel::info,
                                      "请求ID=" + std::to_string(ctx.request.id) + " 商品不支持加购物车，尝试虚拟库存流程");
                            virtualItem = true;
                            break;
                        }

                        if (statusCode == 0 || statusCode == 3) {
                            util::log(util::LogLevel::info,
                                      "请求ID=" + std::to_string(ctx.request.id) + " 商品有货，尝试下单");
                            createCtx.useProxy = useProxy;
                            createCtx.assignedProxy = overrideProxy;
                            createCtx.proxyAffinity = affinity;
                            auto attempt = createOrder(createCtx, *paramsObj);
                            if (attempt.success) {
                                attempt.statusCode = 1;
                            }
                            else if (attempt.shouldContinue || attempt.shouldUpdate) {
                                attempt.statusCode = 2;
                            }
                            else {
                                attempt.statusCode = 3;
                            }
                            if (attempt.response.is_object()) {
                                auto& responseObj = attempt.response.as_object();
                                responseObj["count"] = count;
                                if (count >= 10 && responseObj.if_contains("isContinue")) {
                                    responseObj["isContinue"] = false;
                                }
                            }
                            attempt.attempts = count + std::max(1, attempt.attempts);
                            pickResult = attempt;
                            if (!attempt.shouldContinue) {
                                break;
                            }
                        }
                    } else {
                        util::log(util::LogLevel::warn,
                                  "请求ID=" + std::to_string(ctx.request.id) + " 获取商品库存失败");
                    }

                    ++count;
                    std::this_thread::sleep_for(std::chrono::milliseconds(frequency));
                }

                if (virtualItem && std::chrono::system_clock::now() < endTime && !pickResult) {
                    while (std::chrono::system_clock::now() < endTime) {
                        auto skuInfoUrl = buildSkuInfoUrl(randomDomain(ctx.extension));
                        auto skuInfo = fetchJson(skuInfoUrl, std::chrono::seconds{20}, "获取虚拟库存信息");
                        if (skuInfo && skuInfo->is_object()) {
                            const auto& skuRoot = skuInfo->as_object();
                            const boost::json::object* resultObj = nullptr;
                            if (auto* node = skuRoot.if_contains("result"); node && node->is_object()) {
                                resultObj = &node->as_object();
                            }
                            int stock = 0;
                            if (resultObj) {
                                if (skuIdValue == "0") {
                                    if (auto* node = resultObj->if_contains("itemStock"); node && node->is_int64()) {
                                        stock = static_cast<int>(node->as_int64());
                                    }
                                } else if (auto* node = resultObj->if_contains("skuInfos"); node && node->is_array()) {
                                    for (const auto& skuEntry : node->as_array()) {
                                        if (!skuEntry.is_object()) {
                                            continue;
                                        }
                                        const auto& skuObj = skuEntry.as_object();
                                        if (auto* idNode = skuObj.if_contains("id"); idNode) {
                                            auto candidate = parseString(*idNode);
                                            if (candidate && *candidate == skuIdValue) {
                                                if (auto* stockNode = skuObj.if_contains("stock"); stockNode) {
                                                    if (stockNode->is_int64()) {
                                                        stock = static_cast<int>(stockNode->as_int64());
                                                    } else if (stockNode->is_string()) {
                                                        try {
                                                            stock = std::stoi(std::string(stockNode->as_string().c_str()));
                                                        } catch (const std::exception&) {
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }

                            if (stock > 0) {
                                util::log(util::LogLevel::info,
                                          "请求ID=" + std::to_string(ctx.request.id) + " 虚拟商品有库存，尝试下单");
                                createCtx.useProxy = useProxy;
                                createCtx.assignedProxy = overrideProxy;
                                createCtx.proxyAffinity = affinity;
                                auto attempt = createOrder(createCtx, *paramsObj);
                                if (attempt.success) {
                                    attempt.statusCode = 1;
                                }
                                else if (attempt.shouldContinue || attempt.shouldUpdate) {
                                    attempt.statusCode = 2;
                                }
                                else {
                                    attempt.statusCode = 3;
                                }
                                if (attempt.response.is_object()) {
                                    auto& responseObj = attempt.response.as_object();
                                    responseObj["count"] = count;
                                    if (count >= 10 && responseObj.if_contains("isContinue")) {
                                        responseObj["isContinue"] = false;
                                    }
                                }
                                attempt.attempts = count + std::max(1, attempt.attempts);
                                pickResult = attempt;
                                if (!attempt.shouldContinue) {
                                    break;
                                }
                            }
                        } else {
                            util::log(util::LogLevel::warn,
                                      "请求ID=" + std::to_string(ctx.request.id) + " 获取虚拟库存失败");
                        }

                        ++count;
                        std::this_thread::sleep_for(std::chrono::milliseconds(frequency));
                    }
                }

                if (pickResult) {
                    util::log(util::LogLevel::info,
                              "请求ID=" + std::to_string(ctx.request.id) + " 捡漏结束");
                    finalResult = std::move(*pickResult);
                    break;
                }

                util::log(util::LogLevel::info,
                          "请求ID=" + std::to_string(ctx.request.id) + " 捡漏超时");
                boost::json::object status;
                status["code"] = 400;
                status["message"] = "抢购失败";
                status["description"] = "运行超时";
                boost::json::object response;
                response["status"] = status;
                response["result"] = nullptr;
                finalResult.response = response;
                finalResult.message = "捡漏超时";
                finalResult.attempts = count;
            } while (false);

            boost::asio::post(
                io_,
                [onFinished = std::move(onFinished), result = std::move(finalResult)]() mutable {
                    onFinished(result);
                }
            );
        }
    );
}



void GrabWorkflow::refreshOrderParameters(GrabContext& ctx) {
    if (ctx.quickMode) {
        util::log(util::LogLevel::info,
                  "请求ID=" + std::to_string(ctx.request.id) + " 使用快速模式，跳过重新生成订单参数");
        return;
    }

    auto dataObj = fetchAddOrderData(ctx);
    if (!dataObj) {
        util::log(util::LogLevel::warn,
                  "请求ID=" + std::to_string(ctx.request.id) + " 无法获取下单数据，将尝试使用已有参数");
        return;
    }

    const auto& root = *dataObj;

    // 下钻到 order.result
    const boost::json::object* result = &root;
    if (auto* order = root.if_contains("order"); order && order->is_object()) {
        const auto& orderObj = order->as_object();
        if (auto* res = orderObj.if_contains("result"); res && res->is_object()) {
            result = &res->as_object();
        }
    }

    auto orderParams = quickgrab::util::generateOrderParameters(ctx.request, *result, true);
    if (!orderParams) {
        util::log(util::LogLevel::warn,
                  "请求ID=" + std::to_string(ctx.request.id) + " 解析下单数据失败，将尝试使用已有参数");
        return;
    }

    ctx.request.orderParameters = *orderParams;
    ctx.request.orderParametersRaw = quickgrab::util::stringifyJson(*orderParams);

}

std::optional<boost::json::object> GrabWorkflow::fetchAddOrderData(const GrabContext& ctx) const {
    std::vector<util::HttpClient::Header> headers{
        {"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
        {"Cookie", ctx.request.cookies},
        {"Referer", "https://weidian.com/"},
        {"User-Agent", kDesktopUA}};

    const std::string& affinity = ctx.proxyAffinity.empty() ? ctx.request.threadId : ctx.proxyAffinity;
    bool useProxy = ctx.useProxy;
    std::optional<proxy::ProxyEndpoint> overrideProxy = ctx.assignedProxy;

    try {
        for (int attempt = 0; attempt < 2; ++attempt) {
            try {
                util::log(
                    util::LogLevel::info,
                    "请求ID=" + std::to_string(ctx.request.id) +
                    " 开始获取订单参数"
                    ", affinity=" + affinity +
                    ", useProxy=" + (useProxy ? "true" : "false") +
                    (overrideProxy
                        ? (", overrideProxy=" + overrideProxy->host + ":" + std::to_string(overrideProxy->port))
                        : ", overrideProxy=<none>") +
                    ", timeout=30s, maxRedirects=5"
                );

                    for (const auto& h : headers) {
                        util::log(util::LogLevel::debug, "请求ID=" + std::to_string(ctx.request.id) +
                            " header: " + h.name + " = " + h.value);
                    }
                const auto t0 = std::chrono::steady_clock::now();
                auto response = httpClient_.fetch("GET",
                                                  ctx.request.link,
                                                  headers,
                                                  "",
                                                  affinity,
                                                  std::chrono::seconds{30},
                                                  true,
                                                  5,
                                                  nullptr,
                                                  useProxy,
                                                  overrideProxy ? &*overrideProxy : nullptr);
                const auto t1 = std::chrono::steady_clock::now();
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                quickgrab::util::log(quickgrab::util::LogLevel::info,
                    std::string("HTTP fetch ok: ") + ctx.request.link +
                    " cost=" + std::to_string(ms) + " ms");
                auto data = util::extractDataObject(response.body());
                if (!data || !data->is_object()) {
                    return std::nullopt;
                }
                return data->as_object();
            } catch (const util::ProxyError& ex) {
                util::log(util::LogLevel::warn,
                          std::string{"请求ID="} + std::to_string(ctx.request.id) +
                              " 获取下单页面失败: " + ex.what());
                bool retried = false;
                if (overrideProxy) {
                    util::log(util::LogLevel::info,
                              std::string{"请求ID="} + std::to_string(ctx.request.id) + " 指定代理 " + overrideProxy->host +
                                  ":" + std::to_string(overrideProxy->port) + " 失败(" +
                                  std::to_string(ex.status()) + "), 将尝试代理池或直连重试获取下单页面");
                    overrideProxy.reset();
                    retried = true;
                } else if (useProxy) {
                    util::log(util::LogLevel::info,
                              std::string{"请求ID="} + std::to_string(ctx.request.id) +
                                  " 代理连接失败(" + std::to_string(ex.status()) + "), 将改用直连重试获取下单页面");
                    useProxy = false;
                    retried = true;
                }
                if (!retried) {
                    throw;
                }
            }
        }
    } catch (const util::ProxyError& ex) {
        util::log(util::LogLevel::warn,
                  std::string{"请求ID="} + std::to_string(ctx.request.id) +
                      " 获取下单页面失败: " + ex.what());
        return std::nullopt;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  std::string{"请求ID="} + std::to_string(ctx.request.id) +
                      " 获取下单页面失败: " + ex.what());
        return std::nullopt;
    }

    return std::nullopt;
}




boost::beast::http::request<boost::beast::http::string_body>
GrabWorkflow::buildPost(const std::string& url,
                        const GrabContext& ctx,
                        const std::string& body) const {
    auto schemePos = url.find("://");
    auto hostStart = schemePos == std::string::npos ? 0 : schemePos + 3;
    auto pathPos = url.find('/', hostStart);
    std::string host = pathPos == std::string::npos ? url.substr(hostStart) : url.substr(hostStart, pathPos - hostStart);
    std::string target = pathPos == std::string::npos ? "/" : url.substr(pathPos);
    std::string scheme = schemePos == std::string::npos ? "https" : url.substr(0, schemePos);

    boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, target, 11};
    req.set(boost::beast::http::field::host, host);
    req.set(boost::beast::http::field::content_type, "application/x-www-form-urlencoded;charset=UTF-8");
    req.set(boost::beast::http::field::user_agent, kUserAgent);
    req.set(boost::beast::http::field::referer, kReferer);
    req.set(boost::beast::http::field::cookie, ctx.request.cookies);
    req.set("X-Quick-Scheme", scheme);
    req.body() = body;
    req.prepare_payload();
    return req;
}



} // namespace quickgrab::workflow










