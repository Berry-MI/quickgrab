#pragma once

#include "quickgrab/model/Result.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"

#include <optional>
#include <string>
#include <chrono>

namespace sql {
class ResultSet;
}

namespace quickgrab::repository {

class ResultsRepository {
public:
    explicit ResultsRepository(MySqlConnectionPool& pool);

    void insertResult(const model::Result& result);
    std::optional<model::Result> findById(int resultId);
    void deleteById(int resultId);

private:
    model::Result mapRow(sql::ResultSet& rs);
    std::chrono::system_clock::time_point parseTimestamp(const std::string& value);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
