#pragma once

#include \"quickgrab/proxy/ProxyPool.hpp\"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace quickgrab::service {

class ProxyService {
public:
    ProxyService(boost::asio::io_context& io, proxy::ProxyPool& pool);

    void scheduleRefresh(std::function<std::vector<proxy::ProxyEndpoint>()> callback,
                         std::chrono::minutes cadence);
    void addProxies(std::vector<proxy::ProxyEndpoint> proxies);
    std::vector<proxy::ProxyEndpoint> listProxies() const;

private:
    void doRefresh();

    boost::asio::io_context& io_;
    proxy::ProxyPool& pool_;
    std::function<std::vector<proxy::ProxyEndpoint>()> refreshCallback_;
    std::chrono::minutes cadence_{0};
    std::unique_ptr<boost::asio::steady_timer> timer_;
    mutable std::mutex snapshotMutex_;
    std::vector<proxy::ProxyEndpoint> snapshot_;
};

} // namespace quickgrab::service

