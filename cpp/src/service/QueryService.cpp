#include "quickgrab/service/QueryService.hpp"
#include "quickgrab/util/Logging.hpp"

#include <exception>
#include <optional>
#include <string>

namespace quickgrab::service {

QueryService::QueryService(repository::RequestsRepository& requests,
                           repository::ResultsRepository& results)
    : requests_(requests)
    , results_(results) {}

std::vector<model::Request> QueryService::listPending(int limit) {
    return requests_.findPending(limit);
}

std::optional<model::Result> QueryService::getResultById(int resultId) {
    return results_.findById(resultId);
}

bool QueryService::deleteRequestById(int requestId) {
    try {
        requests_.deleteById(requestId);
        return true;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  "删除抢购请求失败 id=" + std::to_string(requestId) + " error=" + ex.what());
        return false;
    }
}

bool QueryService::deleteResultById(int resultId) {
    try {
        results_.deleteById(resultId);
        return true;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn,
                  "删除抢购结果失败 id=" + std::to_string(resultId) + " error=" + ex.what());
        return false;
    }
}

bool QueryService::checkCookies(const std::string& cookies) const {
    if (cookies.empty()) {
        return false;
    }
    const auto pairPos = cookies.find('=');
    const auto separator = cookies.find(';');
    return pairPos != std::string::npos && separator != std::string::npos && pairPos < separator;
}

} // namespace quickgrab::service
