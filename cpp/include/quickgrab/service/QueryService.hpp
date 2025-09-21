#pragma once

#include "quickgrab/repository/BuyersRepository.hpp"
#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"

#include <optional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quickgrab::service {

class QueryService {
public:
    QueryService(repository::RequestsRepository& requests,
                 repository::ResultsRepository& results,
                 repository::BuyersRepository& buyers);

    std::vector<model::Request> listPending(int limit);
    std::optional<model::Result> getResultById(int resultId);
    std::vector<model::Request> getRequestsByFilters(const std::optional<std::string>& keyword,
                                                     const std::optional<int>& buyerId,
                                                     const std::optional<int>& type,
                                                     const std::optional<int>& status,
                                                     std::string_view order,
                                                     int offset,
                                                     int limit);
    std::vector<model::Result> getResultsByFilters(const std::optional<std::string>& keyword,
                                                   const std::optional<int>& buyerId,
                                                   const std::optional<int>& type,
                                                   const std::optional<int>& status,
                                                   std::string_view order,
                                                   int offset,
                                                   int limit);
    bool deleteRequestById(int requestId);
    bool deleteResultById(int resultId);
    bool checkCookies(const std::string& cookies) const;
    std::vector<model::Buyer> getAllBuyers();

private:
    static std::pair<std::string_view, std::string_view> resolveRequestOrder(std::string_view order);
    static std::pair<std::string_view, std::string_view> resolveResultOrder(std::string_view order);

    repository::RequestsRepository& requests_;
    repository::ResultsRepository& results_;
    repository::BuyersRepository& buyers_;
};

} // namespace quickgrab::service
