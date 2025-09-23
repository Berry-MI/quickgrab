#pragma once

#include "quickgrab/proxy/KdlProxyClient.hpp"
#include "quickgrab/proxy/ProxyPool.hpp"
#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/service/MailService.hpp"
#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/workflow/GrabWorkflow.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <optional>

namespace quickgrab::service {

class GrabService {
public:
    GrabService(boost::asio::io_context& io,
                boost::asio::thread_pool& worker,
                repository::RequestsRepository& requests,
                repository::ResultsRepository& results,
                util::HttpClient& client,
                proxy::ProxyPool& proxies,
                MailService& mailService);

    void setProxyConfig(proxy::KdlProxyConfig config);

    void processPending();
    std::optional<int> handleRequest(const model::Request& request);

private:
    void executeRequest(model::Request request);
    void executeGrab(model::Request request);
    void handleResult(const model::Request& request, const workflow::GrabResult& result);
    long computeAdjustedLatency(const model::Request& request) const;
    long computeProcessingTime(const model::Request& request) const;
    long computeSchedulingTime() const;
    std::optional<proxy::ProxyEndpoint> fetchProxyForRequest(const model::Request& request);
    bool requestWantsProxy(const boost::json::object& extension) const;

    boost::asio::io_context& io_;
    boost::asio::thread_pool& worker_;
    repository::RequestsRepository& requests_;
    repository::ResultsRepository& results_;
    util::HttpClient& httpClient_;
    proxy::ProxyPool& proxyPool_;
    MailService& mailService_;
    std::unique_ptr<workflow::GrabWorkflow> workflow_;
    std::atomic<long> adjustedFactor_;
    long processingTime_;
    std::chrono::system_clock::time_point updateTime_;
    std::chrono::system_clock::time_point prestartTime_;
    long schedulingTime_;
    mutable std::mutex metricsMutex_;
    std::optional<proxy::KdlProxyConfig> proxyConfig_;
    mutable std::mutex proxyMutex_;
};

} // namespace quickgrab::service
