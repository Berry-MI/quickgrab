#include "quickgrab/repository/MySqlConnectionPool.hpp"
#include "quickgrab/util/Logging.hpp"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>

namespace quickgrab::repository {

namespace {
sql::ConnectOptionsMap buildOptions(const DatabaseConfig& config) {
    sql::ConnectOptionsMap options;
    options["hostName"] = config.host;
    options["port"] = config.port;
    options["userName"] = config.user;
    options["password"] = config.password;
    options["OPT_RECONNECT"] = true;
    options["CLIENT_MULTI_STATEMENTS"] = true;
    return options;
}
}

MySqlConnectionPool::MySqlConnectionPool(DatabaseConfig config)
    : config_(std::move(config))
    , driver_(sql::mysql::get_mysql_driver_instance()) {
    if (config_.host.empty()) {
        config_.host = "127.0.0.1";
    }
    if (config_.database.empty()) {
        throw std::runtime_error("Database name must be provided in configuration");
    }
    if (config_.poolSize == 0) {
        config_.poolSize = 4;
    }
}

std::unique_ptr<sql::Connection> MySqlConnectionPool::createConnection() {
    sql::ConnectOptionsMap options = buildOptions(config_);
    std::unique_ptr<sql::Connection> connection(driver_->connect(options));
    connection->setSchema(config_.database);
    if (!config_.charset.empty()) {
        connection->setClientOption("characterSetResults", config_.charset.c_str());
        connection->setClientOption("characterSet", config_.charset.c_str());
    }
    return connection;
}

void MySqlConnectionPool::ConnectionDeleter::operator()(sql::Connection* conn) const noexcept {
    if (pool && conn) {
        pool->release(conn);
    }
}

std::shared_ptr<sql::Connection> MySqlConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto predicate = [this]() { return !idle_.empty() || currentSize_ < config_.poolSize; };
    cv_.wait(lock, predicate);

    if (!idle_.empty()) {
        std::unique_ptr<sql::Connection> conn = std::move(idle_.back());
        idle_.pop_back();
        return std::shared_ptr<sql::Connection>(conn.release(), ConnectionDeleter{this});
    }

    std::unique_ptr<sql::Connection> conn = createConnection();
    ++currentSize_;
    return std::shared_ptr<sql::Connection>(conn.release(), ConnectionDeleter{this});
}

void MySqlConnectionPool::release(sql::Connection* conn) {
    std::unique_ptr<sql::Connection> holder(conn);
    bool healthy = true;
    try {
        if (!holder->isValid()) {
            holder->reconnect();
            holder->setSchema(config_.database);
        }
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::warn, std::string{"MySQL reconnect failed: "} + ex.what());
        healthy = false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (healthy) {
        idle_.push_back(std::move(holder));
    } else {
        if (currentSize_ > 0) {
            --currentSize_;
        }
    }
    lock.unlock();
    cv_.notify_one();
}

} // namespace quickgrab::repository
