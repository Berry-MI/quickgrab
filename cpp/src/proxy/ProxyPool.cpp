#include "quickgrab/proxy/ProxyPool.hpp"

#include <algorithm>
#include <chrono>

namespace quickgrab::proxy {
namespace {

constexpr std::size_t kMaxAffinityProxies = 2;

bool compareProxy(const ProxyEndpoint& lhs, const ProxyEndpoint& rhs) {
    if (lhs.latency == rhs.latency) {
        return lhs.nextAvailable < rhs.nextAvailable;
    }
    return lhs.latency < rhs.latency;
}

bool sameProxy(const ProxyEndpoint& lhs, const ProxyEndpoint& rhs) {
    return lhs.host == rhs.host && lhs.port == rhs.port && lhs.username == rhs.username && lhs.password == rhs.password;
}

void sortByPreference(std::vector<ProxyEndpoint>& proxies) {
    std::stable_sort(proxies.begin(), proxies.end(), compareProxy);
}

} // namespace

ProxyPool::ProxyPool(std::chrono::seconds cooldown)
    : cooldown_(cooldown) {}

std::optional<ProxyEndpoint> ProxyPool::acquire(const std::string& affinityKey) {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);

    auto stateIt = sticky_.find(affinityKey);
    if (stateIt != sticky_.end() && !stateIt->second.proxies.empty()) {
        auto& state = stateIt->second;
        std::size_t previousSize = state.proxies.size();
        for (auto it = pool_.begin(); state.proxies.size() < kMaxAffinityProxies && it != pool_.end();) {
            if (it->nextAvailable > now) {
                ++it;
                continue;
            }
            ProxyEndpoint proxy = *it;
            it = pool_.erase(it);
            proxy.nextAvailable = now + cooldown_;
            state.proxies.push_back(std::move(proxy));
        }
        if (state.proxies.empty()) {
            sticky_.erase(stateIt);
        } else {
            if (state.proxies.size() > previousSize) {
                sortByPreference(state.proxies);
                if (previousSize == 0) {
                    state.cursor = state.proxies.size() > 1 ? 1 : 0;
                } else {
                    state.cursor %= state.proxies.size();
                }
            }
            if (!state.proxies.empty()) {
                auto index = state.cursor % state.proxies.size();
                ProxyEndpoint proxy = state.proxies[index];
                proxy.nextAvailable = now + cooldown_;
                state.proxies[index].nextAvailable = proxy.nextAvailable;
                state.cursor = (index + 1) % state.proxies.size();
                return proxy;
            }
        }
    }

    AffinityState state;
    for (auto it = pool_.begin(); state.proxies.size() < kMaxAffinityProxies && it != pool_.end();) {
        if (it->nextAvailable > now) {
            ++it;
            continue;
        }
        ProxyEndpoint proxy = *it;
        it = pool_.erase(it);
        proxy.nextAvailable = now + cooldown_;
        state.proxies.push_back(std::move(proxy));
    }

    if (state.proxies.empty()) {
        return std::nullopt;
    }

    sortByPreference(state.proxies);
    ProxyEndpoint selected = state.proxies.front();
    selected.nextAvailable = now + cooldown_;
    state.proxies.front().nextAvailable = selected.nextAvailable;
    state.cursor = state.proxies.size() > 1 ? 1 : 0;
    sticky_[affinityKey] = std::move(state);
    return selected;
}

void ProxyPool::reportSuccess(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount = 0;
    proxy.nextAvailable = std::chrono::steady_clock::now() + cooldown_;
    std::scoped_lock lock(mutex_);
    auto stateIt = sticky_.find(affinityKey);
    if (stateIt != sticky_.end()) {
        auto& state = stateIt->second;
        auto match = std::find_if(state.proxies.begin(), state.proxies.end(),
                                  [&](const ProxyEndpoint& entry) { return sameProxy(entry, proxy); });
        if (match != state.proxies.end()) {
            *match = std::move(proxy);
            return;
        }
    }

    pool_.push_back(std::move(proxy));
    std::stable_sort(pool_.begin(), pool_.end(), compareProxy);
}

void ProxyPool::reportFailure(const std::string& affinityKey, ProxyEndpoint proxy) {
    proxy.failureCount += 1;
    proxy.nextAvailable = std::chrono::steady_clock::now() + cooldown_ * (1 + proxy.failureCount);
    std::scoped_lock lock(mutex_);
    auto stateIt = sticky_.find(affinityKey);
    if (stateIt != sticky_.end()) {
        auto& state = stateIt->second;
        auto match = std::find_if(state.proxies.begin(), state.proxies.end(),
                                  [&](const ProxyEndpoint& entry) { return sameProxy(entry, proxy); });
        if (match != state.proxies.end()) {
            state.proxies.erase(match);
            if (state.proxies.empty()) {
                sticky_.erase(stateIt);
            } else {
                state.cursor %= state.proxies.size();
            }
        }
    }

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
        auto& state = it->second;
        for (auto proxyIt = state.proxies.begin(); proxyIt != state.proxies.end();) {
            if (proxyIt->nextAvailable <= now) {
                pool_.push_back(std::move(*proxyIt));
                proxyIt = state.proxies.erase(proxyIt);
            } else {
                ++proxyIt;
            }
        }
        if (state.proxies.empty()) {
            it = sticky_.erase(it);
        } else {
            state.cursor %= state.proxies.size();
            ++it;
        }
    }
    std::stable_sort(pool_.begin(), pool_.end(), compareProxy);
}

} // namespace quickgrab::proxy

