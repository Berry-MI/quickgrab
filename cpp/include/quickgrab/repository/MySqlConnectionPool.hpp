#pragma once

#include "quickgrab/repository/DatabaseConfig.hpp"

#include <mysqlx/xdevapi.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace quickgrab::repository {

class MySqlConnectionPool {
public:
    explicit MySqlConnectionPool(DatabaseConfig config);

    std::shared_ptr<mysqlx::Session> acquire();

    const std::string& schemaName() const noexcept { return config_.database; }

private:
    struct SessionDeleter {
        MySqlConnectionPool* pool;
        void operator()(mysqlx::Session* session) const noexcept;
    };

    std::unique_ptr<mysqlx::Session> createSession();
    void release(mysqlx::Session* session);

    DatabaseConfig config_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<mysqlx::Session>> idle_;
    unsigned int currentSize_{};
};

} // namespace quickgrab::repository
