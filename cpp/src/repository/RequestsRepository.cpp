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

#include <ctime>


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

std::chrono::system_clock::time_point parseDateTimeValue(const mysqlx::Value& value) {
    if (value.isNull()) {
        return std::chrono::system_clock::now();
    }
    try {
        return parseDateTimeString(value.get<std::string>());
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to parse datetime column: "} + ex.what());

        return std::chrono::system_clock::now();
    }
}


std::string readString(const mysqlx::Value& value) {

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


double readDouble(const mysqlx::Value& value) {

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


boost::json::value parseJsonColumn(const mysqlx::Value& value, const std::string& column) {

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

RequestsRepository::RequestsRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}


model::Request RequestsRepository::mapRow(mysqlx::Row row) {

    std::size_t index = 0;
    auto next = [&row, &index]() -> mysqlx::Value { return row[index++]; };

    model::Request request{};
    request.id = next().get<int>();
    request.deviceId = next().get<int>();
    request.buyerId = next().get<int>();

    auto value = next();
    request.threadId = readString(value);
    value = next();
    request.link = readString(value);
    value = next();
    request.cookies = readString(value);
    value = next();
    request.orderInfo = parseJsonColumn(value, "order_info");
    value = next();
    request.userInfo = parseJsonColumn(value, "user_info");
    value = next();
    request.orderTemplate = parseJsonColumn(value, "order_template");
    value = next();
    request.message = readString(value);
    value = next();
    request.idNumber = readString(value);
    value = next();
    request.keyword = readString(value);
    value = next();
    request.startTime = parseDateTimeValue(value);
    value = next();
    request.endTime = parseDateTimeValue(value);

    request.quantity = next().get<int>();
    request.delay = next().get<int>();
    request.frequency = next().get<int>();
    request.type = next().get<int>();
    request.status = next().get<int>();

    value = next();
    request.orderParameters = parseJsonColumn(value, "order_parameters");
    value = next();
    request.actualEarnings = readDouble(value);
    value = next();
    request.estimatedEarnings = readDouble(value);
    value = next();
    request.extension = parseJsonColumn(value, "extension");

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

            .set("updated_at", formatTimestamp(std::chrono::system_clock::now()))

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
            .set("updated_at", formatTimestamp(std::chrono::system_clock::now()))
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

} // namespace quickgrab::repository
