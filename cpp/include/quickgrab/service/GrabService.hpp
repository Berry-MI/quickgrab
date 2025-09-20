#pragma once

#include "quickgrab/proxy/ProxyPool.hpp"
#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/service/MailService.hpp"
#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/workflow/GrabWorkflow.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <chrono>
#include <memory>
#include <string>

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

    void processPending();

private:
    void executeRequest(const model::Request& request);
    void handleResult(const model::Request& request, const workflow::GrabResult& result);

    boost::asio::io_context& io_;
    boost::asio::thread_pool& worker_;
    repository::RequestsRepository& requests_;
    repository::ResultsRepository& results_;
    util::HttpClient& httpClient_;
    proxy::ProxyPool& proxyPool_;
    MailService& mailService_;
    std::unique_ptr<workflow::GrabWorkflow> workflow_;
};

} // namespace quickgrab::service
