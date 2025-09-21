#include "quickgrab/repository/MySqlConnectionPool.hpp"
#include "quickgrab/util/Logging.hpp"

#include <mysqlx/xdevapi.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace quickgrab::repository {

namespace {
std::string normalizeHost(const std::string& host) {
    return host.empty() ? std::string{"127.0.0.1"} : host;

}
} // namespace

MySqlConnectionPool::MySqlConnectionPool(DatabaseConfig config)
    : config_(std::move(config)) {
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

std::unique_ptr<mysqlx::Session> MySqlConnectionPool::createSession() {
    try {

        auto session = std::make_unique<mysqlx::Session>(
            mysqlx::SessionOption::HOST, normalizeHost(config_.host),
            mysqlx::SessionOption::PORT, static_cast<unsigned int>(config_.port),
            mysqlx::SessionOption::USER, config_.user,
            mysqlx::SessionOption::PWD, config_.password);
        if (!config_.charset.empty()) {
            session->sql("SET NAMES '" + config_.charset + "'").execute();
        }
        if (!config_.database.empty()) {
            session->sql("USE `" + config_.database + "`").execute();
        }

        return session;
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Create MySQL session failed: "} + err.what());
        throw;
    }
}

void MySqlConnectionPool::SessionDeleter::operator()(mysqlx::Session* session) const noexcept {
    if (pool && session) {
        pool->release(session);
    }
}

std::shared_ptr<mysqlx::Session> MySqlConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto predicate = [this]() { return !idle_.empty() || currentSize_ < config_.poolSize; };
    cv_.wait(lock, predicate);

    if (!idle_.empty()) {
        std::unique_ptr<mysqlx::Session> session = std::move(idle_.back());
        idle_.pop_back();
        return std::shared_ptr<mysqlx::Session>(session.release(), SessionDeleter{this});
    }

    std::unique_ptr<mysqlx::Session> session = createSession();
    ++currentSize_;
    return std::shared_ptr<mysqlx::Session>(session.release(), SessionDeleter{this});
}

void MySqlConnectionPool::release(mysqlx::Session* session) {
    std::unique_ptr<mysqlx::Session> holder(session);

    bool healthy = static_cast<bool>(holder);


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
