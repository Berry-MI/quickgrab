#include "quickgrab/service/QueryService.hpp"
#include "quickgrab/util/Logging.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace quickgrab::service {

QueryService::QueryService(repository::RequestsRepository& requests,
                           repository::ResultsRepository& results,
                           repository::BuyersRepository& buyers)
    : requests_(requests)
    , results_(results)
    , buyers_(buyers) {}

std::vector<model::Request> QueryService::listPending(int limit) {
    return requests_.findPending(limit);
}

std::optional<model::Result> QueryService::getResultById(int resultId) {
    return results_.findById(resultId);
}

std::vector<model::Request> QueryService::getRequestsByFilters(const std::optional<std::string>& keyword,
                                                               const std::optional<int>& buyerId,
                                                               const std::optional<int>& type,
                                                               const std::optional<int>& status,
                                                               std::string_view order,
                                                               int offset,
                                                               int limit) {
    auto [column, direction] = resolveRequestOrder(order);
    return requests_.findByFilters(keyword,
                                   buyerId,
                                   type,
                                   status,
                                   column,
                                   direction,
                                   std::max(0, offset),
                                   std::max(0, limit));
}

std::vector<model::Result> QueryService::getResultsByFilters(const std::optional<std::string>& keyword,
                                                             const std::optional<int>& buyerId,
                                                             const std::optional<int>& type,
                                                             const std::optional<int>& status,
                                                             std::string_view order,
                                                             int offset,
                                                             int limit) {
    auto [column, direction] = resolveResultOrder(order);
    return results_.findByFilters(keyword,
                                  buyerId,
                                  type,
                                  status,
                                  column,
                                  direction,
                                  std::max(0, offset),
                                  std::max(0, limit));
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

std::vector<model::Buyer> QueryService::getAllBuyers() {
    return buyers_.findAll();
}

std::pair<std::string_view, std::string_view> QueryService::resolveRequestOrder(std::string_view order) {
    if (order == "start_time_asc") {
        return {"start_time", "ASC"};
    }
    if (order == "start_time_desc") {
        return {"start_time", "DESC"};
    }
    return {"id", "DESC"};
}

std::pair<std::string_view, std::string_view> QueryService::resolveResultOrder(std::string_view order) {
    if (order == "end_time_asc") {
        return {"end_time", "ASC"};
    }
    if (order == "end_time_desc") {
        return {"end_time", "DESC"};
    }
    return {"id", "DESC"};
}

} // namespace quickgrab::service
