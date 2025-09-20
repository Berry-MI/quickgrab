#include \"quickgrab/workflow/GrabWorkflow.hpp\"
#include \"quickgrab/util/JsonUtil.hpp\"

#include <cmath>
#include <thread>
#include <cctype>
#include <algorithm>

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <random>
#include <boost/asio/post.hpp>

namespace quickgrab::workflow {
namespace {
constexpr int kMaxRetries = 3;
constexpr char kUserAgent[] = "Android/9 WDAPP(WDBuyer/7.6.2) Thor/2.3.25";
constexpr char kReferer[] = "https://android.weidian.com/";

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
                           util::HttpClient& httpClient,
                           proxy::ProxyPool& proxyPool)
    : io_(io)
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
            result.payload = json;
            if (json.is_object()) {
                const auto& obj = json.as_object();
                if (auto status = obj.if_contains("status")) {
                    if (status->is_object()) {
                        auto& statusObj = status->as_object();
                        result.message = statusObj.if_contains("description") ? statusObj["description"].as_string().c_str() : "";
                        result.statusCode = static_cast<int>(statusObj.if_contains("code") ? statusObj["code"].as_int64() : result.statusCode);
                    }
                }
                result.success = obj.if_contains("isSuccess") && obj["isSuccess"].as_int64() == 1;
                result.shouldUpdate = obj.if_contains("isUpdate") && obj["isUpdate"].as_bool();
                result.shouldContinue = obj.if_contains("isContinue") && obj["isContinue"].as_bool();
            }
            return result;
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"CreateOrder attempt failed: "} + ex.what());
            if (attempt == kMaxRetries) {
                result.success = false;
                result.message = ex.what();
                return result;
            }
            auto waitTime = std::chrono::milliseconds(static_cast<int>(std::pow(2.0, attempt) * 100));
            std::this_thread::sleep_for(waitTime);
        }
    }
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
            if (json.is_object() && json.as_object().if_contains("result")) {
                result.payload = json.as_object()["result"];
                result.success = true;
                return result;
            }
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"ReConfirmOrder attempt failed: "} + ex.what());
        }
        if (attempt < kMaxRetries) {
            auto waitTime = std::chrono::milliseconds(static_cast<int>(std::pow(2.0, attempt) * 80));
            std::this_thread::sleep_for(waitTime);
        }
    }
    result.success = false;
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
                if (confirm.success && confirm.payload.is_object()) {
                    auto& confirmObj = confirm.payload.as_object();
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










