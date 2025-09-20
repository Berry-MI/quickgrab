#include \"quickgrab/repository/RequestsRepository.hpp\"
#include \"quickgrab/util/JsonUtil.hpp\"
#include \"quickgrab/util/Logging.hpp\"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <boost/json.hpp>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace quickgrab::repository {
namespace {
boost::json::value parseJsonColumn(const std::string& text, const std::string& column) {
    if (text.empty()) {
        return boost::json::object{};
    }
    try {
        return quickgrab::util::parseJson(text);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, "JSON parse failed on column " + column + ": " + ex.what());
        return boost::json::object{};
    }
}

std::chrono::system_clock::time_point parseDateTime(const std::string& input) {
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
}

RequestsRepository::RequestsRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

model::Request RequestsRepository::mapRow(sql::ResultSet& rs) {
    auto readString = [&rs](const std::string& column) {
        auto sqlString = rs.getString(column);
        return rs.wasNull() ? std::string{} : static_cast<std::string>(sqlString);
    };

    auto readDouble = [&rs](const std::string& column) {
        double value = rs.getDouble(column);
        return rs.wasNull() ? 0.0 : value;
    };

    model::Request request;
    request.id = rs.getInt("id");
    request.deviceId = rs.getInt("device_id");
    request.buyerId = rs.getInt("buyer_id");
    request.threadId = readString("thread_id");
    request.link = readString("link");
    request.cookies = readString("cookies");
    request.orderInfo = parseJsonColumn(readString("order_info"), "order_info");
    request.userInfo = parseJsonColumn(readString("user_info"), "user_info");
    request.orderTemplate = parseJsonColumn(readString("order_template"), "order_template");
    request.message = readString("message");
    request.idNumber = readString("id_number");
    request.keyword = readString("keyword");
    request.startTime = parseDateTime(readString("start_time"));
    request.endTime = parseDateTime(readString("end_time"));
    request.quantity = rs.getInt("quantity");
    request.delay = rs.getInt("delay");
    request.frequency = rs.getInt("frequency");
    request.type = rs.getInt("type");
    request.status = rs.getInt("status");
    request.orderParameters = parseJsonColumn(readString("order_parameters"), "order_parameters");
    request.actualEarnings = readDouble("actual_earnings");
    request.estimatedEarnings = readDouble("estimated_earnings");
    request.extension = parseJsonColumn(readString("extension"), "extension");
    return request;
}

std::vector<model::Request> RequestsRepository::findPending(int limit) {
    std::vector<model::Request> requests;
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(
            "SELECT id, device_id, buyer_id, thread_id, link, cookies, order_info, user_info, order_template, message, id_number, keyword, start_time, end_time, quantity, delay, frequency, type, status, order_parameters, actual_earnings, estimated_earnings, extension FROM requests WHERE status = 0 ORDER BY start_time ASC LIMIT ?"));
        stmt->setInt(1, limit);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        while (rs->next()) {
            requests.emplace_back(mapRow(*rs));
        }
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Query pending requests failed: "} + ex.what());
        throw;
    }
    return requests;
}

void RequestsRepository::updateStatus(int requestId, int status) {
    auto connection = pool_.acquire();
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(
            "UPDATE requests SET status = ?, updated_at = NOW() WHERE id = ?"));
        stmt->setInt(1, status);
        stmt->setInt(2, requestId);
        stmt->executeUpdate();
    } catch (const sql::SQLException& ex) {
        util::log(util::LogLevel::error, std::string{"Update request status failed: "} + ex.what());
        throw;
    }
}

} // namespace quickgrab::repository
