#pragma once

#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"

#include <optional>
#include <string>
#include <vector>

namespace quickgrab::service {

class QueryService {
public:
    QueryService(repository::RequestsRepository& requests,
                 repository::ResultsRepository& results);

    std::vector<model::Request> listPending(int limit);
    std::optional<model::Result> getResultById(int resultId);
    bool deleteRequestById(int requestId);
    bool deleteResultById(int resultId);
    bool checkCookies(const std::string& cookies) const;

private:
    repository::RequestsRepository& requests_;
    repository::ResultsRepository& results_;
};

} // namespace quickgrab::service
