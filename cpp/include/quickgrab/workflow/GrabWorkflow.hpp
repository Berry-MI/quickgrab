#pragma once

#include "quickgrab/model/Request.hpp"
#include "quickgrab/proxy/ProxyPool.hpp"
#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace quickgrab::workflow {

struct GrabResult {
    bool success{};
    bool shouldUpdate{};
    bool shouldContinue{};
    boost::json::value response;
    std::string message;
    std::string error;
    int statusCode{};
    int attempts{};
};

struct GrabContext {
    model::Request request;
    boost::json::object extension;
    std::string domain;
    std::chrono::steady_clock::time_point start;
    int retryCount{0};
    long adjustedFactor{10};
    long processingTime{19};
    bool quickMode{false};
    bool steadyOrder{false};
    bool autoPick{false};
    bool useProxy{false};
    std::string proxyAffinity;
};

class GrabWorkflow {
public:
    GrabWorkflow(boost::asio::io_context& io,
                 boost::asio::thread_pool& worker,
                 util::HttpClient& httpClient,
                 proxy::ProxyPool& proxyPool);

    void run(const model::Request& request,
             std::function<void(const GrabResult&)> onFinished);

private:
    using TimerPtr = std::shared_ptr<boost::asio::steady_timer>;

    void prepareContext(const model::Request& request, GrabContext& ctx);

    GrabResult createOrder(const GrabContext& ctx, const boost::json::object& payload);
    GrabResult reConfirmOrder(const GrabContext& ctx, const boost::json::object& payload);
    void scheduleExecution(GrabContext ctx,
                           std::function<void(const GrabResult&)> onFinished);

    void refreshOrderParameters(GrabContext& ctx);
    std::optional<boost::json::object> fetchAddOrderData(const GrabContext& ctx) const;

    boost::beast::http::request<boost::beast::http::string_body>
    buildPost(const std::string& url,
              const GrabContext& ctx,
              const std::string& body) const;

    boost::json::object buildBasePayload(const GrabContext& ctx) const;

    boost::asio::io_context& io_;
    boost::asio::thread_pool& worker_;
    util::HttpClient& httpClient_;
    proxy::ProxyPool& proxyPool_;
};

} // namespace quickgrab::workflow
