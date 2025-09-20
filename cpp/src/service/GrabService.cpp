#include \"quickgrab/service/GrabService.hpp\"
#include \"quickgrab/model/Result.hpp\"
#include \"quickgrab/util/Logging.hpp\"

#include <boost/asio/post.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <algorithm>
#include <functional>
#include <optional>
#include <thread>

namespace quickgrab::service {

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
    stored.createdAt = std::chrono::system_clock::now();
    stored.status = std::to_string(result.statusCode);
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
