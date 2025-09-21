#pragma once

#include \"quickgrab/model/Request.hpp\"
#include \"quickgrab/repository/MySqlConnectionPool.hpp\"

#include <mysqlx/xdevapi.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quickgrab::repository {

class RequestsRepository {
public:
    explicit RequestsRepository(MySqlConnectionPool& pool);

    std::vector<model::Request> findPending(int limit);
    std::vector<model::Request> findByFilters(const std::optional<std::string>& keyword,
                                              const std::optional<int>& buyerId,
                                              const std::optional<int>& type,
                                              const std::optional<int>& status,
                                              std::string_view orderColumn,
                                              std::string_view orderDirection,
                                              int offset,
                                              int limit);
    void updateStatus(int requestId, int status);
    void updateThreadId(int requestId, const std::string& threadId);
    void deleteById(int requestId);

private:
    model::Request mapRow(mysqlx::Row row);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
