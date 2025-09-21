#include "quickgrab/server/Router.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace quickgrab::server {
namespace {
std::string normalizeMethod(std::string method) {
    std::transform(method.begin(), method.end(), method.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return method;
}
}

void Router::addRoute(std::string method, std::string path, Handler handler) {
    RouteEntry entry;
    entry.method = normalizeMethod(std::move(method));
    entry.path = std::move(path);
    entry.handler = std::move(handler);

    std::string token;
    std::ostringstream regexBuilder;
    regexBuilder << '^';

    std::istringstream iss(entry.path);
    while (std::getline(iss, token, '/')) {
        if (token.empty()) {
            continue;
        }
        regexBuilder << '/';
        if (token.front() == ':') {
            entry.tokens.push_back(token.substr(1));
            regexBuilder << "([^/]+)";
        } else {
            regexBuilder << token;
        }
    }

    if (!entry.path.empty() && entry.path.back() == '/') {
        regexBuilder << '/';
    }

    regexBuilder << "/?$";
    entry.pattern = std::regex(regexBuilder.str());

    routes_.push_back(std::move(entry));
}

Router::Handler Router::resolve(const std::string& method,
                                const std::string& path,
                                std::unordered_map<std::string, std::string>& params) const {
    auto normalized = normalizeMethod(method);
    const std::string* pathToMatch = &path;
    std::string strippedPath;
    if (auto queryPos = path.find('?'); queryPos != std::string::npos) {
        strippedPath = path.substr(0, queryPos);
        if (strippedPath.empty()) {
            strippedPath = "/";
        }
        pathToMatch = &strippedPath;
    }
    for (const auto& entry : routes_) {
        if (!entry.method.empty() && !normalized.empty() && entry.method != normalized) {
            continue;
        }

        std::smatch match;
        if (std::regex_match(*pathToMatch, match, entry.pattern)) {
            params.clear();
            for (std::size_t i = 0; i < entry.tokens.size(); ++i) {
                if (i + 1 < match.size()) {
                    params.emplace(entry.tokens[i], match[i + 1].str());
                }
            }
            return entry.handler;
        }
    }

    return nullptr;
}

} // namespace quickgrab::server
