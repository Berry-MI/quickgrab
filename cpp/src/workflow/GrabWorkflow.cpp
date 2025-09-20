#include \"quickgrab/workflow/GrabWorkflow.hpp\"
#include \"quickgrab/util/JsonUtil.hpp\"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <random>
#include <string_view>
#include <thread>

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/asio/post.hpp>

namespace quickgrab::workflow {
namespace {
constexpr int kMaxRetries = 3;
constexpr char kUserAgent[] = "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25";
constexpr char kReferer[] = "https://android.weidian.com/";

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
}

GrabResult GrabWorkflow::createOrder(const GrabContext& ctx, const boost::json::object& payload) {
    GrabResult result;
    auto requestBody = quickgrab::util::stringifyJson(payload);

    auto req = buildPost("https://" + ctx.domain + "/vbuy/CreateOrder/1.0", ctx, toQuery(requestBody));

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        try {
            auto response = httpClient_.fetch(req, ctx.request.threadId, std::chrono::seconds{30});
            result.statusCode = static_cast<int>(response.result());
            auto json = quickgrab::util::parseJson(response.body());
            result.response = json;
            result.attempts = attempt + 1;
            if (json.is_object()) {
                const auto& obj = json.as_object();
                if (auto* status = obj.if_contains("status"); status && status->is_object()) {
                    const auto& statusObj = status->as_object();
                    if (auto* description = statusObj.if_contains("description"); description && description->is_string()) {
                        result.message = std::string(description->as_string());
                    }
                    if (auto* code = statusObj.if_contains("code"); code && code->is_int64()) {
                        result.statusCode = static_cast<int>(code->as_int64());
                    }
                }

                result.success = obj.if_contains("isSuccess") && obj.at("isSuccess").as_int64() == 1;
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
                if (!result.shouldContinue) {
                    return result;
                }
            } else {
                result.message = "未知响应";
                result.shouldContinue = false;
                return result;
            }
            return result;
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"CreateOrder attempt failed: "} + ex.what());
            if (attempt == kMaxRetries) {
                result.success = false;
                result.error = ex.what();
                result.message.clear();
                return result;
            }
            static thread_local std::mt19937 rng{std::random_device{}()};
            auto base = static_cast<int>(std::pow(2.0, attempt) * 100);
            std::uniform_int_distribution<int> jitter(-base / 5, base / 5);
            auto waitTime = std::chrono::milliseconds(base + jitter(rng));
            std::this_thread::sleep_for(waitTime);
        }
    }
    result.error = "CreateOrder exhausted retries";
    return result;
}

GrabResult GrabWorkflow::reConfirmOrder(const GrabContext& ctx, const boost::json::object& payload) {
    GrabResult result;
    auto requestBody = quickgrab::util::stringifyJson(payload);
    auto req = buildPost("https://" + ctx.domain + "/vbuy/ReConfirmOrder/1.0", ctx, toQuery(requestBody));

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        try {
            auto response = httpClient_.fetch(req, ctx.request.threadId, std::chrono::seconds{20});
            result.statusCode = static_cast<int>(response.result());
            auto json = quickgrab::util::parseJson(response.body());
            result.attempts = attempt + 1;
            if (json.is_object() && json.as_object().if_contains("result")) {
                result.response = json.as_object().at("result");
                result.success = true;
                return result;
            }
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"ReConfirmOrder attempt failed: "} + ex.what());
            if (attempt == kMaxRetries) {
                result.error = ex.what();
            }
        }
        if (attempt < kMaxRetries) {
            auto waitTime = std::chrono::milliseconds(static_cast<int>(std::pow(2.0, attempt) * 80));
            std::this_thread::sleep_for(waitTime);
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
    auto payloadValue = ctx.request.orderParameters;
    boost::json::object payload;
    if (payloadValue.is_object()) {
        payload = payloadValue.as_object();
    } else {
        payload = buildBasePayload(ctx);
    }

    auto delay = computeDelay(ctx);
    util::log(util::LogLevel::info, "请求ID=" + std::to_string(ctx.request.id) +
        (ctx.quickMode ? " [快速模式]" : "") +
        (ctx.steadyOrder ? " [稳定抢购]" : "") +
        (ctx.autoPick ? " [自动选取]" : "") +
        " 将在 " + std::to_string(delay) + "ms 后开始抢购");

    auto timer = std::make_shared<boost::asio::steady_timer>(io_);
    timer->expires_after(std::chrono::milliseconds(delay));
    timer->async_wait([this, ctx = std::move(ctx), onFinished = std::move(onFinished), timer, payload = std::move(payload)](const boost::system::error_code& ec) mutable {
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

        boost::asio::post(worker_, [this, ctx = std::move(ctx), payload = std::move(payload), onFinished = std::move(onFinished)]() mutable {
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
    });
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
    payload["buyer_id"] = ctx.request.buyerId;
    payload["device_id"] = ctx.request.deviceId;
    payload["link"] = ctx.request.link;
    payload["thread_id"] = ctx.request.threadId;
    return payload;
}

} // namespace quickgrab::workflow










