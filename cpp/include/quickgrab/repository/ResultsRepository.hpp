#pragma once

#include \"quickgrab/model/Result.hpp\"
#include \"quickgrab/repository/MySqlConnectionPool.hpp\"

namespace quickgrab::repository {

class ResultsRepository {
public:
    explicit ResultsRepository(MySqlConnectionPool& pool);

    void insertResult(const model::Result& result);

private:
    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
