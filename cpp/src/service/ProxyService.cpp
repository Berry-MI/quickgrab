#include "quickgrab/service/ProxyService.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

namespace quickgrab::service {
namespace {

constexpr std::chrono::milliseconds kLatencyTimeout{1500};

bool proxyLatencyLess(const proxy::ProxyEndpoint& lhs, const proxy::ProxyEndpoint& rhs) {
    if (lhs.latency == rhs.latency) {
        return lhs.host < rhs.host;
    }
    return lhs.latency < rhs.latency;
}

std::chrono::milliseconds measureProxyLatency(const proxy::ProxyEndpoint& endpoint) {
    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::steady_timer timer(io);
        std::chrono::milliseconds latency = std::chrono::milliseconds::max();
        bool connected = false;

        auto start = std::chrono::steady_clock::now();
        auto endpoints = resolver.resolve(endpoint.host, std::to_string(endpoint.port));

        boost::asio::async_connect(socket, endpoints,
                                   [&](const boost::system::error_code& ec,
                                       const boost::asio::ip::tcp::endpoint&) {
                                       if (!ec) {
                                           connected = true;
                                           latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start);
                                           timer.cancel();
                                       }
                                   });

        timer.expires_after(kLatencyTimeout);
        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec) {
                socket.cancel();
            }
        });

        io.run();

        if (connected) {
            boost::system::error_code ec;
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
            return latency;
        }
    } catch (const std::exception&) {
    }
    return kLatencyTimeout * 2;
}

void prepareProxies(std::vector<proxy::ProxyEndpoint>& proxies) {
    const auto now = std::chrono::steady_clock::now();
    for (auto& proxy : proxies) {
        proxy.failureCount = 0;
        proxy.nextAvailable = now;
        if (proxy.latency == std::chrono::milliseconds::max() ||
            proxy.latency <= std::chrono::milliseconds::zero()) {
            proxy.latency = measureProxyLatency(proxy);
        }
    }
    std::stable_sort(proxies.begin(), proxies.end(), proxyLatencyLess);
}

} // namespace

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
    prepareProxies(proxies);
    {
        std::scoped_lock lock(snapshotMutex_);
        snapshot_.insert(snapshot_.end(), proxies.begin(), proxies.end());
        std::stable_sort(snapshot_.begin(), snapshot_.end(), proxyLatencyLess);
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
    prepareProxies(proxies);
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


