#pragma once

#include \"quickgrab/model/Request.hpp\"
#include \"quickgrab/repository/MySqlConnectionPool.hpp\"

#include <string>
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
    void updateThreadId(int requestId, const std::string& threadId);
    void deleteById(int requestId);

private:
    model::Request mapRow(sql::ResultSet& rs);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
