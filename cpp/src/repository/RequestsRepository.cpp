#include "quickgrab/repository/RequestsRepository.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"
#include "quickgrab/repository/SqlUtils.hpp"

#include <mysqlx/common/value.h>
#include <mysqlx/xdevapi.h>

#include <boost/json.hpp>
#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <ctime>

namespace quickgrab::repository {
namespace {
std::chrono::system_clock::time_point fromTm(const std::tm& tm) {
    auto localTm = tm;
    return std::chrono::system_clock::from_time_t(std::mktime(&localTm));
}

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
    auto replaceChar = [&text](char from, char to) {
        std::replace(text.begin(), text.end(), from, to);
    };
    replaceChar('T', ' ');
    auto dotPos = text.find('.');
    if (dotPos != std::string::npos) {
        text = text.substr(0, dotPos);
    }
    return text;
}

std::chrono::system_clock::time_point parseDateTimeString(const std::string& input,
                                                          const std::chrono::system_clock::time_point& fallback = {}) {
    if (input.empty()) {
        return fallback;
    }
    std::tm tm{};
    std::istringstream iss(input);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        util::log(util::LogLevel::warn, std::string{"Failed to parse datetime text: "} + input);
        return fallback;
    }
    return fromTm(tm);
}

std::chrono::system_clock::time_point parseDateTimeValue(
    const mysqlx::Value& value,
    const std::chrono::system_clock::time_point& fallback = std::chrono::system_clock::now()) {
    if (value.isNull()) {
        return fallback;
    }
    try {
        auto text = sanitizeDateTimeString(valueToString(value));
        return parseDateTimeString(text, fallback);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to parse datetime column: "} + ex.what());
        return fallback;
    }
}

std::string readString(const mysqlx::Value& value) {
    if (value.isNull()) {
        return {};
    }
    try {
        return value.get<std::string>();
    } catch (const std::exception& ex) {
        auto fallback = valueToString(value);
        if (fallback.empty()) {
            util::log(util::LogLevel::warn, std::string{"Failed to read string column: "} + ex.what());
        }
        return fallback;
    }
}

double readDouble(const mysqlx::Value& value) {
    if (value.isNull()) {
        return 0.0;
    }
    try {
        return value.get<double>();
    } catch (const std::exception& ex) {
        try {
            auto text = valueToString(value);
            if (text.empty()) {
                util::log(util::LogLevel::warn, std::string{"Failed to read double column: "} + ex.what());
                return 0.0;
            }
            return std::stod(text);
        } catch (const std::exception& inner) {
            util::log(util::LogLevel::warn, std::string{"Failed to read double column: "} + inner.what());
            return 0.0;
        }
    }
}

boost::json::value parseJsonColumn(const mysqlx::Value& value, const std::string& column) {
    if (value.isNull()) {
        return boost::json::object{};
    }
    try {
        return quickgrab::util::parseJson(valueToString(value));
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

mysqlx::Value makeNullValue() {
    return mysqlx::Value();
}

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

std::vector<model::Request> RequestsRepository::findByFilters(const std::optional<std::string>& keyword,
                                                              const std::optional<int>& buyerId,
                                                              const std::optional<int>& type,
                                                              const std::optional<int>& status,
                                                              std::string_view orderColumn,
                                                              std::string_view orderDirection,
                                                              int offset,
                                                              int limit) {
    std::vector<model::Request> requests;
    auto session = pool_.acquire();
    try {
        std::ostringstream sql;
        sql << "SELECT id, device_id, buyer_id, thread_id, link, cookies, order_info, user_info, order_template, message, "
               "id_number, keyword, start_time, end_time, quantity, delay, frequency, type, status, order_parameters, "
               "actual_earnings, estimated_earnings, extension FROM requests WHERE 1=1";

        std::vector<mysqlx::Value> params;
        if (keyword && !keyword->empty()) {
            sql << " AND user_info LIKE ?";
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
            requests.emplace_back(mapRow(row));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"按条件查询抢购请求失败: "} + err.what());
        throw;
    }
    return requests;
}

int RequestsRepository::insert(const model::Request& request) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("requests");
        auto result = table
                          .insert("device_id",
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
                                  "order_parameters",
                                  "actual_earnings",
                                  "estimated_earnings",
                                  "extension")
                          .values(request.deviceId,
                                  request.buyerId,
                                  request.threadId,
                                  request.link,
                                  request.cookies,
                                  jsonOrNull(request.orderInfo),
                                  jsonOrNull(request.userInfo),
                                  jsonOrNull(request.orderTemplate),
                                  request.message,
                                  request.idNumber,
                                  request.keyword,
                                  toTimestampValue(request.startTime),
                                  toTimestampValue(request.endTime),
                                  request.quantity,
                                  request.delay,
                                  request.frequency,
                                  request.type,
                                  request.status,
                                  jsonOrNull(request.orderParameters),
                                  request.actualEarnings,
                                  request.estimatedEarnings,
                                  jsonOrNull(request.extension))
                          .execute();
        return static_cast<int>(result.getAutoIncrementValue());
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"Insert request failed: "} + err.what());
        throw;
    }
}

} // namespace quickgrab::repository
