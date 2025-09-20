#include \"quickgrab/service/GrabService.hpp\"
#include \"quickgrab/model/Result.hpp\"
#include \"quickgrab/util/Logging.hpp\"

#include <boost/asio/post.hpp>
#include <boost/json.hpp>
#include <chrono>

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
    , workflow_(std::make_unique<workflow::GrabWorkflow>(io_, worker_, httpClient_, proxyPool_)) {}

void GrabService::processPending() {
    boost::asio::post(worker_, [this]() {
        auto pending = requests_.findPending(50);
        boost::asio::post(io_, [this, pending = std::move(pending)]() mutable {
            for (auto& request : pending) {
                executeRequest(request);
            }
        });
    });
}

void GrabService::executeRequest(const model::Request& request) {
    util::log(util::LogLevel::info, "开始处理抢购请求 id=" + std::to_string(request.id));
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
}

} // namespace quickgrab::service
