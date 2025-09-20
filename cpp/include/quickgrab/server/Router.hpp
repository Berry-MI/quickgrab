#pragma once

#include "quickgrab/server/RequestContext.hpp"

#include <functional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace quickgrab::server {

class Router {
public:
    using Handler = std::function<void(RequestContext&)>;

    void addRoute(std::string method, std::string path, Handler handler);
    Handler resolve(const std::string& method,
                    const std::string& path,
                    std::unordered_map<std::string, std::string>& params) const;

private:
    struct RouteEntry {
        std::string method;
        std::string path;
        std::regex pattern;
        std::vector<std::string> tokens;
        Handler handler;
    };

    std::vector<RouteEntry> routes_;
};

} // namespace quickgrab::server
