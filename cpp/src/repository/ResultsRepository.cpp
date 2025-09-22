#include "quickgrab/repository/ResultsRepository.hpp"
#include "quickgrab/repository/SqlUtils.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <mysqlx/common/value.h>
#include <mysqlx/xdevapi.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <ctime>
#include <unordered_map>

namespace quickgrab::repository {
namespace {
std::string valueToString(const mysqlx::Value& value) {
    if (value.isNull()) {
        return {};
    }
    try {
        return value.get<std::string>();
    } catch (const std::exception&) {
        try {
            std::ostringstream oss;
            value.print(oss);
            return oss.str();
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Failed to stringify value: "} + ex.what());
            return {};
        }
    }
}

std::string sanitizeDateTimeString(std::string text) {
    if (text.empty()) {
        return text;
    }
    if (text.front() == '\'' && text.back() == '\'' && text.size() > 1) {
        text = text.substr(1, text.size() - 2);
    }
    if (text.front() == '"' && text.back() == '"' && text.size() > 1) {
        text = text.substr(1, text.size() - 2);
    }
    std::replace(text.begin(), text.end(), 'T', ' ');
    auto dotPos = text.find('.');
    if (dotPos != std::string::npos) {
        text = text.substr(0, dotPos);
    }
    return text;
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

mysqlx::Value makeNullValue() { return mysqlx::Value(); }

mysqlx::Value toTimestampValue(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) {
        return makeNullValue();
    }
    return mysqlx::Value(formatTimestamp(tp));
}

mysqlx::Value jsonOrNull(const boost::json::value& value) {
    if (value.is_null()) {
        return makeNullValue();
    }
    return mysqlx::Value(quickgrab::util::stringifyJson(value));
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
    } catch (const std::exception&) {
        try {
            auto text = valueToString(value);
            if (text.empty()) {
                return 0.0;
            }
            return std::stod(text);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Failed to read double column: "} + ex.what());
            return 0.0;
        }
    }
}

boost::json::value parseJsonColumn(const mysqlx::Value& value, const std::string& column) {
    if (value.isNull()) {
        return boost::json::value();
    }
    try {
        return quickgrab::util::parseJson(valueToString(value));
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, "JSON parse failed on column " + column + ": " + ex.what());
        return boost::json::value();
    }
}

std::chrono::system_clock::time_point fromTm(const std::tm& tm) {
    auto localTm = tm;
    return std::chrono::system_clock::from_time_t(std::mktime(&localTm));
}

std::chrono::system_clock::time_point parseDateTimeValue(
    const mysqlx::Value& value,
    const std::chrono::system_clock::time_point& fallback = std::chrono::system_clock::now()) {
    if (value.isNull()) {
        return fallback;
    }
    try {
        auto text = sanitizeDateTimeString(valueToString(value));
        if (text.empty()) {
            return fallback;
        }
        std::tm tm{};
        std::istringstream iss(text);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (iss.fail()) {
            util::log(util::LogLevel::warn, "Failed to parse datetime text value");
            return fallback;
        }
        return fromTm(tm);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to parse datetime column: "} + ex.what());
        return fallback;
    }
}

int readInt(const mysqlx::Value& value, const std::string& column, int defaultValue = 0) {
    if (value.isNull()) {
        return defaultValue;
    }
    try {
        return static_cast<int>(value.get<std::int64_t>());
    } catch (const std::exception&) {
        try {
            auto text = valueToString(value);
            if (text.empty()) {
                return defaultValue;
            }
            return std::stoi(text);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn,
                      "Failed to read int column " + column + ": " + ex.what());
            return defaultValue;
        }
    }
}

using ColumnIndex = std::unordered_map<std::string, std::size_t>;

ColumnIndex buildColumnIndex(const mysqlx::Row& row) {
    ColumnIndex index;
    index.reserve(row.colCount());
    for (std::size_t i = 0; i < row.colCount(); ++i) {
        try {
            auto name = row.getColumn(i).getColumnName();
            index.emplace(std::move(name), i);
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn,
                      std::string{"Failed to read column metadata: "} + ex.what());
        }
    }
    return index;
}

const mysqlx::Value* findValue(const mysqlx::Row& row, const ColumnIndex& index, std::string_view column) {
    auto it = index.find(std::string(column));
    if (it == index.end()) {
        return nullptr;
    }
    return &row[it->second];
}
} // namespace

ResultsRepository::ResultsRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

void ResultsRepository::insertResult(const model::Result& result) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("results");
        auto responsePayload = result.responseMessage.is_null() ? result.payload : result.responseMessage;
        table.insert("request_id",
                    "device_id",
                    "buyer_id",
                    "thread_id",
                    "link",
                    "cookies",
                    "order_info",
                    "user_info",
                    "order_template",
                    "message",
                    "id_number",
                    "keyword",
                    "start_time",
                    "end_time",
                    "quantity",
                    "delay",
                    "frequency",
                    "type",
                    "status",
                    "response_message",
                    "actual_earnings",
                    "estimated_earnings",
                    "extension",
                    "created_at")
            .values(result.requestId,
                    result.deviceId,
                    result.buyerId,
                    result.threadId,
                    result.link,
                    result.cookies,
                    jsonOrNull(result.orderInfo),
                    jsonOrNull(result.userInfo),
                    jsonOrNull(result.orderTemplate),
                    result.message,
                    result.idNumber,
                    result.keyword,
                    toTimestampValue(result.startTime),
                    toTimestampValue(result.endTime),
                    result.quantity,
                    result.delay,
                    result.frequency,
                    result.type,
                    result.status,
                    jsonOrNull(responsePayload),
                    result.actualEarnings,
                    result.estimatedEarnings,
                    jsonOrNull(result.extension),
                    toTimestampValue(result.createdAt))
            .execute();
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Insert result failed: "} + err.what());
        throw;
    }
}

std::optional<model::Result> ResultsRepository::findById(int resultId) {
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT id, request_id, device_id, buyer_id, thread_id, link, cookies, order_info, user_info, order_template, "
               "message, id_number, keyword, start_time, end_time, quantity, delay, frequency, type, status, response_message, "
               "actual_earnings, estimated_earnings, extension, created_at FROM results WHERE id = :id";
        auto rows = session->sql(sql.str()).bind("id", resultId).execute();
        for (mysqlx::Row row : rows) {
            return mapDetailedRow(row);
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

std::vector<model::Result> ResultsRepository::findByFilters(const std::optional<std::string>& keyword,
                                                            const std::optional<int>& buyerId,
                                                            const std::optional<int>& type,
                                                            const std::optional<int>& status,
                                                            std::string_view orderColumn,
                                                            std::string_view orderDirection,
                                                            int offset,
                                                            int limit) {
    std::vector<model::Result> results;
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT id, request_id, device_id, buyer_id, thread_id, link, cookies, order_info, user_info, order_template, "
               "message, id_number, keyword, start_time, end_time, quantity, delay, frequency, type, status, response_message, "
               "actual_earnings, estimated_earnings, extension, created_at FROM results WHERE 1=1";

        std::vector<mysqlx::Value> params;
        if (keyword && !keyword->empty()) {
            sql << " AND (user_info LIKE ?)";
            params.emplace_back("%" + *keyword + "%");
        }
        if (buyerId) {
            sql << " AND buyer_id = ?";
            params.emplace_back(*buyerId);
        }
        if (type) {
            sql << " AND type = ?";
            params.emplace_back(*type);
        }
        if (status) {
            sql << " AND status = ?";
            params.emplace_back(*status);
        }
        sql << " ORDER BY " << orderColumn << ' ' << orderDirection;
        sql << buildLimitOffsetClause(limit, offset);

        auto stmt = session->sql(sql.str());
        for (const auto& param : params) {
            stmt.bind(param);
        }

        auto rows = stmt.execute();
        for (mysqlx::Row row : rows) {
            results.emplace_back(mapDetailedRow(row));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"按条件查询抢购结果失败: "} + err.what());
        throw;
    }
    return results;
}

std::vector<ResultsRepository::AggregatedStats> ResultsRepository::getStatistics(const std::optional<int>& buyerId,
                                                                                const std::optional<std::string>& startTime,
                                                                                const std::optional<std::string>& endTime) {
    std::vector<AggregatedStats> stats;
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT type, IFNULL(SUM(successCount), 0) AS successCount, IFNULL(SUM(failureCount), 0) AS failureCount, "
               "IFNULL(SUM(exceptionCount), 0) AS exceptionCount, IFNULL(SUM(totalCount), 0) AS totalCount, "
               "IFNULL(SUM(successEarnings), 0) AS successEarnings, IFNULL(SUM(failureEarnings), 0) AS failureEarnings, "
               "IFNULL(SUM(exceptionEarnings), 0) AS exceptionEarnings, IFNULL(SUM(totalEarnings), 0) AS totalEarnings "
               "FROM ( SELECT CASE WHEN type IS NULL THEN 'total' ELSE CAST(type AS CHAR) END AS type, "
               "SUM(CASE WHEN status = 1 THEN 1 ELSE 0 END) AS successCount, "
               "SUM(CASE WHEN status = 2 THEN 1 ELSE 0 END) AS failureCount, "
               "SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) AS exceptionCount, COUNT(*) AS totalCount, "
               "SUM(CASE WHEN status = 1 THEN actual_earnings ELSE 0 END) AS successEarnings, "
               "SUM(CASE WHEN status = 2 THEN actual_earnings ELSE 0 END) AS failureEarnings, "
               "SUM(CASE WHEN status = 3 THEN actual_earnings ELSE 0 END) AS exceptionEarnings, SUM(actual_earnings) AS totalEarnings "
               "FROM results";
        if (buyerId || startTime || endTime) {
            sql << " WHERE 1=1";
        }
        if (buyerId) {
            sql << " AND buyer_id = :buyerId";
        }
        if (startTime) {
            sql << " AND start_time >= :startTime";
        }
        if (endTime) {
            sql << " AND start_time <= :endTime";
        }
        sql << " GROUP BY type WITH ROLLUP UNION ALL SELECT '1' AS type, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL SELECT '2' AS type, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL SELECT '3' AS type, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL SELECT 'total' AS type, 0, 0, 0, 0, 0, 0, 0, 0 ) AS stats GROUP BY type";

        auto stmt = session->sql(sql.str());
        if (buyerId) {
            stmt.bind("buyerId", *buyerId);
        }
        if (startTime) {
            stmt.bind("startTime", *startTime);
        }
        if (endTime) {
            stmt.bind("endTime", *endTime);
        }
        auto rows = stmt.execute();
        for (mysqlx::Row row : rows) {
            AggregatedStats entry{};
            entry.type = readString(row[0]);
            entry.successCount = row[1].isNull() ? 0 : row[1].get<std::int64_t>();
            entry.failureCount = row[2].isNull() ? 0 : row[2].get<std::int64_t>();
            entry.exceptionCount = row[3].isNull() ? 0 : row[3].get<std::int64_t>();
            entry.totalCount = row[4].isNull() ? 0 : row[4].get<std::int64_t>();
            entry.successEarnings = readDouble(row[5]);
            entry.failureEarnings = readDouble(row[6]);
            entry.exceptionEarnings = readDouble(row[7]);
            entry.totalEarnings = readDouble(row[8]);
            stats.emplace_back(std::move(entry));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"查询统计数据失败: "} + err.what());
        throw;
    }
    return stats;
}

std::vector<ResultsRepository::DailyStat> ResultsRepository::getDailyStats(const std::optional<int>& buyerId,
                                                                           const std::optional<int>& status) {
    std::vector<DailyStat> items;
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT dates.date AS date, IFNULL(COUNT(r.id), 0) AS total, IFNULL(SUM(r.actual_earnings), 0) AS earnings "
               "FROM (SELECT DATE_SUB(CURDATE(), INTERVAL seq DAY) AS date FROM seq_0_to_14) dates "
               "LEFT JOIN results r ON DATE(r.start_time) = dates.date";
        if (buyerId || status) {
            sql << " AND 1=1";
        }
        if (buyerId) {
            sql << " AND r.buyer_id = :buyerId";
        }
        if (status) {
            sql << " AND r.status = :status";
        }
        sql << " GROUP BY dates.date ORDER BY dates.date";

        auto stmt = session->sql(sql.str());
        if (buyerId) {
            stmt.bind("buyerId", *buyerId);
        }
        if (status) {
            stmt.bind("status", *status);
        }
        auto rows = stmt.execute();
        for (mysqlx::Row row : rows) {
            DailyStat stat{};
            stat.date = readString(row[0]);
            stat.total = row[1].isNull() ? 0 : row[1].get<std::int64_t>();
            stat.earnings = readDouble(row[2]);
            items.emplace_back(std::move(stat));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"查询每日统计失败: "} + err.what());
        throw;
    }
    return items;
}

std::vector<ResultsRepository::HourlyStat> ResultsRepository::getHourlyStats(const std::optional<int>& buyerId,
                                                                             const std::optional<int>& status) {
    std::vector<HourlyStat> items;
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT hours.hour AS hour, IFNULL(COUNT(r.id), 0) AS total, IFNULL(SUM(r.actual_earnings), 0) AS earnings "
               "FROM (SELECT 0 AS hour UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL "
               "SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9 UNION ALL SELECT 10 UNION ALL SELECT 11 UNION ALL SELECT 12 UNION ALL SELECT 13 UNION ALL SELECT 14 UNION ALL SELECT 15 UNION ALL SELECT 16 UNION ALL SELECT 17 UNION ALL SELECT 18 UNION ALL SELECT 19 UNION ALL SELECT 20 UNION ALL SELECT 21 UNION ALL SELECT 22 UNION ALL SELECT 23) hours "
               "LEFT JOIN results r ON HOUR(r.start_time) = hours.hour AND r.start_time >= NOW() - INTERVAL 1 DAY";
        if (buyerId || status) {
            sql << " AND 1=1";
        }
        if (buyerId) {
            sql << " AND r.buyer_id = :buyerId";
        }
        if (status) {
            sql << " AND r.status = :status";
        }
        sql << " GROUP BY hours.hour ORDER BY hours.hour";

        auto stmt = session->sql(sql.str());
        if (buyerId) {
            stmt.bind("buyerId", *buyerId);
        }
        if (status) {
            stmt.bind("status", *status);
        }
        auto rows = stmt.execute();
        for (mysqlx::Row row : rows) {
            HourlyStat stat{};
            stat.hour = row[0].isNull() ? 0 : row[0].get<int>();
            stat.total = row[1].isNull() ? 0 : row[1].get<std::int64_t>();
            stat.earnings = readDouble(row[2]);
            items.emplace_back(std::move(stat));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"查询每小时统计失败: "} + err.what());
        throw;
    }
    return items;
}

model::Result ResultsRepository::mapRow(mysqlx::Row row) {
    return mapDetailedRow(std::move(row));
}

model::Result ResultsRepository::mapDetailedRow(mysqlx::Row row) {
    model::Result result{};
    if (row.colCount() == 0) {
        return result;
    }

    auto index = buildColumnIndex(row);

    if (const auto* value = findValue(row, index, "id")) {
        result.id = readInt(*value, "id");
    }
    if (const auto* value = findValue(row, index, "request_id")) {
        result.requestId = readInt(*value, "request_id");
    }
    if (const auto* value = findValue(row, index, "device_id")) {
        result.deviceId = readInt(*value, "device_id");
    }
    if (const auto* value = findValue(row, index, "buyer_id")) {
        result.buyerId = readInt(*value, "buyer_id");
    }
    if (const auto* value = findValue(row, index, "thread_id")) {
        result.threadId = readString(*value);
    }
    if (const auto* value = findValue(row, index, "link")) {
        result.link = readString(*value);
    }
    if (const auto* value = findValue(row, index, "cookies")) {
        result.cookies = readString(*value);
    }
    if (const auto* value = findValue(row, index, "order_info")) {
        result.orderInfo = parseJsonColumn(*value, "order_info");
    }
    if (const auto* value = findValue(row, index, "user_info")) {
        result.userInfo = parseJsonColumn(*value, "user_info");
    }
    if (const auto* value = findValue(row, index, "order_template")) {
        result.orderTemplate = parseJsonColumn(*value, "order_template");
    }
    if (const auto* value = findValue(row, index, "message")) {
        result.message = readString(*value);
    }
    if (const auto* value = findValue(row, index, "id_number")) {
        result.idNumber = readString(*value);
    }
    if (const auto* value = findValue(row, index, "keyword")) {
        result.keyword = readString(*value);
    }
    if (const auto* value = findValue(row, index, "start_time")) {
        result.startTime = parseDateTimeValue(*value, {});
    }
    if (const auto* value = findValue(row, index, "end_time")) {
        result.endTime = parseDateTimeValue(*value, {});
    }
    if (const auto* value = findValue(row, index, "quantity")) {
        result.quantity = readInt(*value, "quantity");
    }
    if (const auto* value = findValue(row, index, "delay")) {
        result.delay = readInt(*value, "delay");
    }
    if (const auto* value = findValue(row, index, "frequency")) {
        result.frequency = readInt(*value, "frequency");
    }
    if (const auto* value = findValue(row, index, "type")) {
        result.type = readInt(*value, "type");
    }
    if (const auto* value = findValue(row, index, "status")) {
        result.status = readInt(*value, "status");
    }
    if (const auto* value = findValue(row, index, "response_message")) {
        result.responseMessage = parseJsonColumn(*value, "response_message");
        if (result.payload.is_null()) {
            result.payload = result.responseMessage;
        }
    }
    if (const auto* value = findValue(row, index, "actual_earnings")) {
        result.actualEarnings = readDouble(*value);
    }
    if (const auto* value = findValue(row, index, "estimated_earnings")) {
        result.estimatedEarnings = readDouble(*value);
    }
    if (const auto* value = findValue(row, index, "extension")) {
        result.extension = parseJsonColumn(*value, "extension");
    }
    if (const auto* value = findValue(row, index, "payload")) {
        auto payload = parseJsonColumn(*value, "payload");
        if (!payload.is_null()) {
            result.payload = std::move(payload);
        }
    }
    if (const auto* value = findValue(row, index, "created_at")) {
        try {
            result.createdAt = parseTimestamp(valueToString(*value));
        } catch (const std::exception& ex) {
            util::log(util::LogLevel::warn, std::string{"Failed to parse result created_at: "} + ex.what());
        }
    }
    return result;
}

std::chrono::system_clock::time_point ResultsRepository::parseTimestamp(const std::string& value) {
    if (value.empty()) {
        return std::chrono::system_clock::now();
    }
    std::tm tm{};
    std::istringstream iss(sanitizeDateTimeString(value));
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace quickgrab::repository
