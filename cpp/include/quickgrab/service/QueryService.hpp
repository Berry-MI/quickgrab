#pragma once

#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"

#include <vector>

namespace quickgrab::service {

class QueryService {
public:
    QueryService(repository::RequestsRepository& requests,
                 repository::ResultsRepository& results);

    std::vector<model::Request> listPending(int limit);

private:
    repository::RequestsRepository& requests_;
    repository::ResultsRepository& results_;
};

} // namespace quickgrab::service
