#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <chrono>
#include <exception>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>

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
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(
            "INSERT INTO results (request_id, status, payload, created_at ) VALUES (?, ?, ?, ?)"));
        stmt->setInt(1, result.requestId);
        stmt->setString(2, result.status);
        auto payload = util::stringifyJson(result.payload);
        stmt->setString(3, payload);
        stmt->setString(4, formatTimestamp(result.createdAt));
        stmt->executeUpdate();
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Insert result failed: "} + ex.what());
        throw;
    }
}

std::optional<model::Result> ResultsRepository::findById(int resultId) {
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(
            "SELECT id, request_id, status, payload, created_at FROM results WHERE id = ?"));
        stmt->setInt(1, resultId);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return mapRow(*rs);
        }
        return std::nullopt;
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Find result failed: "} + ex.what());
        throw;
    }
}

void ResultsRepository::deleteById(int resultId) {
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            connection->prepareStatement("DELETE FROM results WHERE id = ?"));
        stmt->setInt(1, resultId);
        stmt->executeUpdate();
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Delete result failed: "} + ex.what());
        throw;
    }
}

model::Result ResultsRepository::mapRow(sql::ResultSet& rs) {
    model::Result result;
    result.id = rs.getInt("id");
    result.requestId = rs.getInt("request_id");
    auto status = rs.getString("status");
    result.status = rs.wasNull() ? std::string{} : static_cast<std::string>(status);
    auto payloadText = rs.getString("payload");
    if (!rs.wasNull()) {
        try {
            result.payload = quickgrab::util::parseJson(static_cast<std::string>(payloadText));
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Parse result payload failed: "} + ex.what());
        }
    }
    auto createdText = rs.getString("created_at");
    if (!rs.wasNull()) {
        result.createdAt = parseTimestamp(static_cast<std::string>(createdText));
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
