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
                         proxy::ProxyPool& proxies)
    : io_(io)
    , worker_(worker)
    , requests_(requests)
    , results_(results)
    , httpClient_(client)
    , proxyPool_(proxies)
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
    boost::json::object payload = result.payload.is_object() ? result.payload.as_object() : boost::json::object{};
    payload["message"] = result.message;
    payload["success"] = result.success;
    stored.payload = std::move(payload);

    if (result.success) {
        util::log(util::LogLevel::info, "抢购完成 id=" + std::to_string(request.id));
        requests_.updateStatus(request.id, 1);
    } else if (result.shouldContinue) {
        util::log(util::LogLevel::warn, "抢购请求需继续 id=" + std::to_string(request.id));
        requests_.updateStatus(request.id, 4);
    } else {
        util::log(util::LogLevel::error, "抢购失败 id=" + std::to_string(request.id) + " 原因=" + result.message);
        requests_.updateStatus(request.id, 3);
    }

    results_.insertResult(stored);
}

} // namespace quickgrab::service
