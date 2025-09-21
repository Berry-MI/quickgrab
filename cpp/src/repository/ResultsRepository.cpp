#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <mysqlx/xdevapi.h>

#include <chrono>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <ctime>

namespace quickgrab::repository {
namespace {
std::string formatTimestamp(std::chrono::system_clock::time_point tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
} // namespace

ResultsRepository::ResultsRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

void ResultsRepository::insertResult(const model::Result& result) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("results");
        table.insert("request_id", "status", "payload", "created_at")
            .values(result.requestId,
                    result.status,
                    util::stringifyJson(result.payload),
                    formatTimestamp(result.createdAt))
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Insert result failed: "} + err.what());
        throw;
    }
}

std::optional<model::Result> ResultsRepository::findById(int resultId) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("results");
        mysqlx::RowResult rows = table
            .select("id", "request_id", "status", "payload", "created_at")
            .where("id = :id")
            .bind("id", resultId)
            .execute();
        for (mysqlx::Row row : rows) {
            return mapRow(row);
        }
        return std::nullopt;
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Find result failed: "} + err.what());
        throw;
    }
}

void ResultsRepository::deleteById(int resultId) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("results");
        table.remove()
            .where("id = :id")
            .bind("id", resultId)
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Delete result failed: "} + err.what());
        throw;
    }
}

model::Result ResultsRepository::mapRow(mysqlx::Row row) {
    std::size_t index = 0;
    auto next = [&row, &index]() -> mysqlx::Value { return row[index++]; };

    model::Result result{};
    result.id = next().get<int>();
    result.requestId = next().get<int>();
    auto statusValue = next();
    if (!statusValue.isNull()) {
        try {
            result.status = statusValue.get<std::string>();
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Failed to read result status: "} + ex.what());
        }
    }
    auto payloadValue = next();
    if (!payloadValue.isNull()) {
        try {
            result.payload = quickgrab::util::parseJson(payloadValue.get<std::string>());
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Parse result payload failed: "} + ex.what());
        }
    }
    auto createdValue = next();
    if (!createdValue.isNull()) {
        try {
            result.createdAt = parseTimestamp(createdValue.get<std::string>());
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Failed to parse result timestamp: "} + ex.what());
        }
    }
    return result;
}

std::chrono::system_clock::time_point ResultsRepository::parseTimestamp(const std::string& value) {
    if (value.empty()) {
        return std::chrono::system_clock::now();
    }
    std::tm tm{};
    std::istringstream iss(value);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace quickgrab::repository
