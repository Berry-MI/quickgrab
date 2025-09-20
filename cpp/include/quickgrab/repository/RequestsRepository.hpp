#pragma once

#include \"quickgrab/model/Request.hpp\"
#include \"quickgrab/repository/MySqlConnectionPool.hpp\"

#include <vector>

namespace sql {
class ResultSet;
}

namespace quickgrab::repository {

class RequestsRepository {
public:
    explicit RequestsRepository(MySqlConnectionPool& pool);

    std::vector<model::Request> findPending(int limit);
    void updateStatus(int requestId, int status);

private:
    model::Request mapRow(sql::ResultSet& rs);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
