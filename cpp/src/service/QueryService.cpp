#include \"quickgrab/service/QueryService.hpp\"

namespace quickgrab::service {

QueryService::QueryService(repository::RequestsRepository& requests,
                           repository::ResultsRepository& results)
    : requests_(requests)
    , results_(results) {}

std::vector<model::Request> QueryService::listPending(int limit) {
    return requests_.findPending(limit);
}

} // namespace quickgrab::service
