#include "quickgrab/proxy/ProxyPool.hpp"

#include <algorithm>
#include <chrono>

namespace quickgrab::proxy {
namespace {

bool compareProxy(const ProxyEndpoint& lhs, const ProxyEndpoint& rhs) {
    if (lhs.latency == rhs.latency) {
        return lhs.nextAvailable < rhs.nextAvailable;
    }
    return lhs.latency < rhs.latency;
}

} // namespace

ProxyPool::ProxyPool(std::chrono::seconds cooldown)
    : cooldown_(cooldown) {}

std::optional<ProxyEndpoint> ProxyPool::acquire(const std::string& affinityKey) {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);

    auto it = sticky_.find(affinityKey);
    if (it != sticky_.end()) {
        it->second.nextAvailable = now + cooldown_;
        return it->second;
    }

    for (auto poolIt = pool_.begin(); poolIt != pool_.end(); ++poolIt) {
        if (poolIt->nextAvailable <= now) {
            ProxyEndpoint proxy = *poolIt;
            pool_.erase(poolIt);
            proxy.nextAvailable = now + cooldown_;
            auto inserted = sticky_.insert_or_assign(affinityKey, proxy);
            return inserted.first->second;
        }
    }

    return std::nullopt;
}

void ProxyPool::reportSuccess(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount = 0;
    proxy.nextAvailable = std::chrono::steady_clock::now() + cooldown_;
    std::scoped_lock lock(mutex_);
    sticky_[affinityKey] = std::move(proxy);
}

void ProxyPool::reportFailure(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount += 1;
    proxy.nextAvailable = std::chrono::steady_clock::now() + cooldown_ * (1 + proxy.failureCount);
    std::scoped_lock lock(mutex_);
    sticky_.erase(affinityKey);
    pool_.push_back(std::move(proxy));
    std::stable_sort(pool_.begin(), pool_.end(), compareProxy);
}

void ProxyPool::hydrate(std::vector<ProxyEndpoint> fresh) {
    if (fresh.empty()) {
        return;
    }
    std::scoped_lock lock(mutex_);
    for (auto& proxy : fresh) {
        pool_.push_back(std::move(proxy));
    }
    std::stable_sort(pool_.begin(), pool_.end(), compareProxy);
}

void ProxyPool::tick() {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);
    for (auto it = sticky_.begin(); it != sticky_.end();) {
        if (it->second.nextAvailable <= now) {
            pool_.push_back(std::move(it->second));
            it = sticky_.erase(it);
        } else {
            ++it;
        }
    }
    std::stable_sort(pool_.begin(), pool_.end(), compareProxy);
}

} // namespace quickgrab::proxy

