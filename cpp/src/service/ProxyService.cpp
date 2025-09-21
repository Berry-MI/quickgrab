#include "quickgrab/service/ProxyService.hpp"

#include <boost/asio/steady_timer.hpp>

namespace quickgrab::service {

ProxyService::ProxyService(boost::asio::io_context& io, proxy::ProxyPool& pool)
    : io_(io)
    , pool_(pool) {}

void ProxyService::scheduleRefresh(std::function<std::vector<proxy::ProxyEndpoint>()> callback,
                                   std::chrono::minutes cadence) {
    refreshCallback_ = std::move(callback);
    cadence_ = cadence;
    timer_ = std::make_unique<boost::asio::steady_timer>(io_);
    doRefresh();
}

void ProxyService::addProxies(std::vector<proxy::ProxyEndpoint> proxies) {
    if (proxies.empty()) {
        return;
    }
    {
        std::scoped_lock lock(snapshotMutex_);
        snapshot_.insert(snapshot_.end(), proxies.begin(), proxies.end());
    }
    pool_.hydrate(std::move(proxies));
}

std::vector<proxy::ProxyEndpoint> ProxyService::listProxies() const {
    std::scoped_lock lock(snapshotMutex_);
    return snapshot_;
}

void ProxyService::doRefresh() {
    if (!refreshCallback_) {
        return;
    }
    auto proxies = refreshCallback_();
    {
        std::scoped_lock lock(snapshotMutex_);
        snapshot_ = proxies;
    }
    pool_.hydrate(std::move(proxies));
    timer_->expires_after(cadence_);
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            doRefresh();
        }
    });
}

} // namespace quickgrab::service


