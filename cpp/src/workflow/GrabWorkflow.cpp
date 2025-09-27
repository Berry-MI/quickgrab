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
    long delay = static_cast<long>(needDelay - ctx.adjustedFactor - ctx.processingTime);
    long timeRemaining = std::max<long>(0, static_cast<long>(delta) + delay);
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
    static constexpr std::array<std::string_view, 4> keys{"proxyAffinity", "affinity", "proxyKey", "proxy_key"};
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
    scheduleExecution(std::move(ctx), std::move(onFinished));
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
            result.shouldContinue = retryHint || updateHint;
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

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        const std::string& affinity = ctx.proxyAffinity.empty() ? ctx.request.threadId : ctx.proxyAffinity;
        auto handleFailure = [&](const std::exception& ex) {
            if (attempt == kMaxRetries) {
                result.error = ex.what();
                return false;
            }
            auto waitTime = std::chrono::milliseconds(static_cast<int>(std::pow(2.0, attempt) * 80));
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
            if (json.is_object() && json.as_object().if_contains("result")) {
                result.response = json.as_object().at("result");
                result.success = true;
                return result;
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
            if (!handleFailure(ex)) {
                break;
            }
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"ReConfirmOrder attempt failed: "} + ex.what());
            if (!handleFailure(ex)) {
                break;
            }
        }
    }
    result.success = false;
    if (result.error.empty()) {
        result.error = "ReConfirmOrder exhausted retries";
    }
    return result;
}

void GrabWorkflow::scheduleExecution(GrabContext ctx,
                                     std::function<void(const GrabResult&)> onFinished) {
    auto delay = computeDelay(ctx);
    util::log(util::LogLevel::info, "请求ID=" + std::to_string(ctx.request.id) +
        (ctx.quickMode ? " [快速模式]" : "") +
        (ctx.steadyOrder ? " [稳定抢购]" : "") +
        (ctx.autoPick ? " [自动选取]" : "") +
        " 将在 " + std::to_string(delay) + "ms 后开始抢购");

    auto timer = std::make_shared<boost::asio::steady_timer>(worker_.get_executor());
    timer->expires_after(std::chrono::milliseconds(delay));
    timer->async_wait([this, ctx = std::move(ctx), onFinished = std::move(onFinished), timer](const boost::system::error_code& ec) mutable {
        if (ec) {
            GrabResult cancelled;
            cancelled.success = false;
            cancelled.statusCode = 499;
            cancelled.message = ec.message();
            boost::asio::post(io_, [onFinished = std::move(onFinished), cancelled]() mutable {
                onFinished(cancelled);
            });
            return;
        }

        // 由于定时器绑定到了 worker_ 的执行器，抢购流程从延时等待开始便已经在工作线程
        // 上排队执行。这样可以确保单个请求在 worker_ 池中始终只占用一个线程，避免了
        // 先由 io_context 线程触发再切换到 worker_ 造成的双重占用问题。
        refreshOrderParameters(ctx);
        boost::json::object payload;
        bool hasPayload = false;
        if (ctx.request.orderParameters.is_object()) {
            payload = ctx.request.orderParameters.as_object();
            hasPayload = true;
        } else if (!ctx.request.orderParametersRaw.empty()) {
            try {
                auto parsed = quickgrab::util::parseJson(ctx.request.orderParametersRaw);
                if (parsed.is_object()) {
                    payload = parsed.as_object();
                    hasPayload = true;
                } else {
                    util::log(util::LogLevel::warn,
                              "请求 id=" + std::to_string(ctx.request.id) + " 的订单参数不是 JSON 对象，使用默认模板");
                }
            } catch (const std::exception& ex) {
                util::log(util::LogLevel::warn,
                          "解析订单参数失败 id=" + std::to_string(ctx.request.id) + " error=" + ex.what());
            }
        }
        if (!hasPayload) {
            payload = buildBasePayload(ctx);
        } else {
            ctx.request.orderParameters = payload;
            ctx.request.orderParametersRaw = quickgrab::util::stringifyJson(payload);
        }
        std::cout << "payload = " << boost::json::serialize(payload) << std::endl;
        auto result = createOrder(ctx, payload);
        if (result.shouldContinue || result.shouldUpdate) {
            auto confirm = reConfirmOrder(ctx, payload);
            if (confirm.success && confirm.response.is_object()) {
                auto& confirmObj = confirm.response.as_object();
                if (auto extra = confirmObj.if_contains("extra"); extra && extra->is_object()) {
                    payload["extra"] = *extra;
                }
            }
            auto rerun = createOrder(ctx, payload);
            result = std::move(rerun);
        }

        boost::asio::post(io_, [onFinished = std::move(onFinished), result = std::move(result)]() mutable {
            onFinished(result);
        });
    });
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
                util::log(util::LogLevel::info,
                "请求ID=" + std::to_string(ctx.request.id) +
                    " 开始 fetch: method=GET"
                    ", url=" + ctx.request.link +
                    ", affinity=" + affinity +
                    ", useProxy=" + (useProxy ? "true" : "false") +
                    (overrideProxy
                        ? (", overrideProxy=" + overrideProxy->host + ":" + std::to_string(overrideProxy->port))
                        : ", overrideProxy=<none>") +
                    ", timeout=30s, maxRedirects=5");

                    for (const auto& h : headers) {
                        util::log(util::LogLevel::debug, "请求ID=" + std::to_string(ctx.request.id) +
                            " header: " + h.name + " = " + h.value);
                    }
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

boost::json::object GrabWorkflow::buildBasePayload(const GrabContext& ctx) const {
    boost::json::object payload;
    if (ctx.request.orderParameters.is_object()) {
        return ctx.request.orderParameters.as_object();
    }
    if (!ctx.request.orderParametersRaw.empty()) {
        try {
            auto parsed = quickgrab::util::parseJson(ctx.request.orderParametersRaw);
            if (parsed.is_object()) {
                return parsed.as_object();
            }
        } catch (const std::exception&) {
        }
    }
    payload["buyer_id"] = ctx.request.buyerId;
    payload["device_id"] = ctx.request.deviceId;
    payload["link"] = ctx.request.link;
    payload["thread_id"] = ctx.request.threadId;
    return payload;
}

} // namespace quickgrab::workflow










