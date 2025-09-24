#pragma once

#include "quickgrab/proxy/ProxyPool.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace quickgrab::util {
class HttpClient;
}

namespace quickgrab::proxy {

struct KdlProxyConfig {
    std::string endpoint{"https://dps.kdlapi.com/api/getdps/"};
    std::string secretId;
    std::string signature;
    std::string username;
    std::string password;
    unsigned int batchSize{5};
    std::chrono::minutes refreshInterval{std::chrono::minutes{5}};
};

std::optional<KdlProxyConfig> loadKdlProxyConfig(const std::filesystem::path& path);

std::vector<ProxyEndpoint> fetchKdlProxies(const KdlProxyConfig& config,
                                           util::HttpClient& httpClient);

} // namespace quickgrab::proxy

