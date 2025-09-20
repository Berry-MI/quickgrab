#pragma once

#include "quickgrab/repository/DatabaseConfig.hpp"

#include <cppconn/connection.h>
#include <mysql_driver.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace quickgrab::repository {

class MySqlConnectionPool {
public:
    explicit MySqlConnectionPool(DatabaseConfig config);

    std::shared_ptr<sql::Connection> acquire();

private:
    struct ConnectionDeleter {
        MySqlConnectionPool* pool;
        void operator()(sql::Connection* conn) const noexcept;
    };

    std::unique_ptr<sql::Connection> createConnection();
    void release(sql::Connection* conn);

    DatabaseConfig config_;
    sql::mysql::MySQL_Driver* driver_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<sql::Connection>> idle_;
    unsigned int currentSize_{};
};

} // namespace quickgrab::repository
