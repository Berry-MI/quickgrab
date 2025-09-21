#pragma once

#include "quickgrab/model/Result.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"


#include <mysqlx/xdevapi.h>


#include <chrono>
#include <optional>
#include <string>


namespace quickgrab::repository {

class ResultsRepository {
public:
    explicit ResultsRepository(MySqlConnectionPool& pool);

    void insertResult(const model::Result& result);
    std::optional<model::Result> findById(int resultId);
    void deleteById(int resultId);

private:

    model::Result mapRow(mysqlx::Row row);

    std::chrono::system_clock::time_point parseTimestamp(const std::string& value);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
