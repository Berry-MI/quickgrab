#include "quickgrab/proxy/ProxyPool.hpp"

#include <algorithm>
#include <chrono>

namespace quickgrab::proxy {

ProxyPool::ProxyPool(std::chrono::seconds cooldown)
    : cooldown_(cooldown) {}

std::optional<ProxyEndpoint> ProxyPool::acquire(const std::string& affinityKey) {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);

    auto it = sticky_.find(affinityKey);
    if (it != sticky_.end()) {
        if (it->second.nextAvailable <= now) {
            ProxyEndpoint proxy = it->second;
            sticky_.erase(it);
            return proxy;
        }
    }

    while (!pool_.empty()) {
        if (pool_.front().nextAvailable <= now) {
            ProxyEndpoint proxy = pool_.front();
            pool_.pop_front();
            sticky_.emplace(affinityKey, proxy);
            return proxy;
        }
        break;
    }

    return std::nullopt;
}

void ProxyPool::reportSuccess(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount = 0;
    proxy.nextAvailable = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);
    sticky_.erase(affinityKey);
    pool_.push_back(std::move(proxy));
}

void ProxyPool::reportFailure(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount += 1;
    proxy.nextAvailable = std::chrono::steady_clock::now() + cooldown_ * (1 + proxy.failureCount);
    std::scoped_lock lock(mutex_);
    sticky_.erase(affinityKey);
    pool_.push_back(std::move(proxy));
}

void ProxyPool::hydrate(std::vector<ProxyEndpoint> fresh) {
    std::scoped_lock lock(mutex_);
    for (auto& proxy : fresh) {
        proxy.failureCount = 0;
        proxy.nextAvailable = std::chrono::steady_clock::now();
        pool_.push_back(std::move(proxy));
    }
}

void ProxyPool::tick() {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);
    for (auto it = sticky_.begin(); it != sticky_.end();) {
        if (it->second.nextAvailable <= now) {
            pool_.push_back(it->second);
            it = sticky_.erase(it);
        } else {
            ++it;
        }
    }
    std::stable_sort(pool_.begin(), pool_.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.nextAvailable < rhs.nextAvailable;
    });
}

} // namespace quickgrab::proxy

