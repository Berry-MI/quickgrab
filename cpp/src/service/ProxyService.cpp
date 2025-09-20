#include \"quickgrab/service/ProxyService.hpp\"

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

std::vector<proxy::ProxyEndpoint> ProxyService::listProxies() const {
    // For brevity, expose a copy via callback. In production this should snapshot in a thread-safe way.
    if (refreshCallback_) {
        return refreshCallback_();
    }
    return {};
}

void ProxyService::doRefresh() {
    if (!refreshCallback_) {
        return;
    }
    auto proxies = refreshCallback_();
    pool_.hydrate(std::move(proxies));
    timer_->expires_after(cadence_);
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            doRefresh();
        }
    });
}

} // namespace quickgrab::service


