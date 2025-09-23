#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quickgrab::proxy {

struct ProxyEndpoint {
    std::string host;
    std::uint16_t port{};
    std::string username;
    std::string password;
    std::chrono::steady_clock::time_point nextAvailable{};
    int failureCount{};
    std::chrono::milliseconds latency{std::chrono::milliseconds::max()};
};

class ProxyPool {
public:
    explicit ProxyPool(std::chrono::seconds cooldown);

    std::optional<ProxyEndpoint> acquire(const std::string& affinityKey);
    void reportSuccess(const std::string& affinityKey, ProxyEndpoint proxy);
    void reportFailure(const std::string& affinityKey, ProxyEndpoint proxy);
    void hydrate(std::vector<ProxyEndpoint> fresh);
    void tick();

private:
    std::chrono::seconds cooldown_;
    mutable std::mutex mutex_;
    std::deque<ProxyEndpoint> pool_;
    std::unordered_map<std::string, ProxyEndpoint> sticky_;
};

} // namespace quickgrab::proxy


