#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <mysqlx/xdevapi.h>

#include <boost/json.hpp>
#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

namespace quickgrab::repository {
namespace {
std::chrono::system_clock::time_point parseDateTimeString(const std::string& input) {
    if (input.empty()) {
        return std::chrono::system_clock::now();
    }
    std::tm tm{};
    std::istringstream iss(input);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::chrono::system_clock::time_point fromDateTime(mysqlx::datetime value) {
    std::tm tm{};
    tm.tm_year = value.year - 1900;
    tm.tm_mon = value.month - 1;
    tm.tm_mday = value.day;
    tm.tm_hour = value.hour;
    tm.tm_min = value.minute;
    tm.tm_sec = value.second;
    auto base = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return base + std::chrono::microseconds(value.microsecond);
}

std::chrono::system_clock::time_point parseDateTimeValue(mysqlx::Value value) {
    if (value.isNull()) {
        return std::chrono::system_clock::now();
    }
    switch (value.getType()) {
    case mysqlx::Value::Type::DATETIME:
        return fromDateTime(value.get<mysqlx::datetime>());
    case mysqlx::Value::Type::STRING:
        return parseDateTimeString(value.get<std::string>());
    default:
        return std::chrono::system_clock::now();
    }
}

std::string readString(mysqlx::Value value) {
    if (value.isNull()) {
        return {};
    }
    try {
        return value.get<std::string>();
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to read string column: "} + ex.what());
        return {};
    }
}

double readDouble(mysqlx::Value value) {
    if (value.isNull()) {
        return 0.0;
    }
    try {
        return value.get<double>();
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to read double column: "} + ex.what());
        return 0.0;
    }
}

boost::json::value parseJsonColumn(mysqlx::Value value, const std::string& column) {
    if (value.isNull()) {
        return boost::json::object{};
    }
    try {
        return quickgrab::util::parseJson(value.get<std::string>());
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, "JSON parse failed on column " + column + ": " + ex.what());
        return boost::json::object{};
    }
}
} // namespace

RequestsRepository::RequestsRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

model::Request RequestsRepository::mapRow(const mysqlx::Row& row) {
    std::size_t index = 0;
    auto next = [&row, &index]() -> mysqlx::Value { return row[index++]; };

    model::Request request{};
    request.id = next().get<int>();
    request.deviceId = next().get<int>();
    request.buyerId = next().get<int>();
    request.threadId = readString(next());
    request.link = readString(next());
    request.cookies = readString(next());
    request.orderInfo = parseJsonColumn(next(), "order_info");
    request.userInfo = parseJsonColumn(next(), "user_info");
    request.orderTemplate = parseJsonColumn(next(), "order_template");
    request.message = readString(next());
    request.idNumber = readString(next());
    request.keyword = readString(next());
    request.startTime = parseDateTimeValue(next());
    request.endTime = parseDateTimeValue(next());
    request.quantity = next().get<int>();
    request.delay = next().get<int>();
    request.frequency = next().get<int>();
    request.type = next().get<int>();
    request.status = next().get<int>();
    request.orderParameters = parseJsonColumn(next(), "order_parameters");
    request.actualEarnings = readDouble(next());
    request.estimatedEarnings = readDouble(next());
    request.extension = parseJsonColumn(next(), "extension");
    return request;
}

std::vector<model::Request> RequestsRepository::findPending(int limit) {
    std::vector<model::Request> requests;
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("requests");

        mysqlx::TableSelect select = table
            .select("id", "device_id", "buyer_id", "thread_id", "link", "cookies", "order_info", "user_info",
                    "order_template", "message", "id_number", "keyword", "start_time", "end_time", "quantity",
                    "delay", "frequency", "type", "status", "order_parameters", "actual_earnings",
                    "estimated_earnings", "extension")
            .where("status = :status")
            .orderBy("start_time ASC");

        if (limit > 0) {
            select.limit(static_cast<std::size_t>(limit));
        }

        mysqlx::RowResult rows = select.bind("status", 0).execute();

        for (mysqlx::Row row : rows) {
            requests.emplace_back(mapRow(row));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Query pending requests failed: "} + err.what());
        throw;
    }
    return requests;
}

void RequestsRepository::updateStatus(int requestId, int status) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("requests");
        table.update()
            .set("status", status)
            .set("updated_at", mysqlx::Expr("NOW()"))
            .where("id = :id")
            .bind("id", requestId)
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Update request status failed: "} + err.what());
        throw;
    }
}

void RequestsRepository::updateThreadId(int requestId, const std::string& threadId) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("requests");
        table.update()
            .set("thread_id", threadId)
            .set("updated_at", mysqlx::Expr("NOW()"))
            .where("id = :id")
            .bind("id", requestId)
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Update request thread failed: "} + err.what());
        throw;
    }
}

void RequestsRepository::deleteById(int requestId) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("requests");
        table.remove()
            .where("id = :id")
            .bind("id", requestId)
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Delete request failed: "} + err.what());
        throw;
    }
}

void RequestsRepository::updateThreadId(int requestId, const std::string& threadId) {
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(
            "UPDATE requests SET thread_id = ?, updated_at = NOW() WHERE id = ?"));
        stmt->setString(1, threadId);
        stmt->setInt(2, requestId);
        stmt->executeUpdate();
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Update request thread failed: "} + ex.what());
        throw;
    }
}

void RequestsRepository::deleteById(int requestId) {
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            connection->prepareStatement("DELETE FROM requests WHERE id = ?"));
        stmt->setInt(1, requestId);
        stmt->executeUpdate();
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Delete request failed: "} + ex.what());
        throw;
    }
}

} // namespace quickgrab::repository
