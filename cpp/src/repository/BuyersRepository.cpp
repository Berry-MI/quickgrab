#include "quickgrab/repository/BuyersRepository.hpp"
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
            util::log(util::LogLevel::warn, std::string{"字符串化列失败: "} + ex.what());
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
    auto dot = text.find('.');
    if (dot != std::string::npos) {
        text = text.substr(0, dot);
    }
    return text;
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
            util::log(util::LogLevel::warn, std::string{"读取字符串列失败: "} + ex.what());
        }
        return fallback;
    }
}

int readInt(const mysqlx::Value& value) {
    if (value.isNull()) {
        return 0;
    }
    try {
        return value.get<int>();
    } catch (const std::exception& ex) {
        try {
            auto text = valueToString(value);
            if (text.empty()) {
                util::log(util::LogLevel::warn, std::string{"读取整数列失败: "} + ex.what());
                return 0;
            }
            return std::stoi(text);
        } catch (const std::exception& inner) {
            util::log(util::LogLevel::warn, std::string{"读取整数列失败: "} + inner.what());
            return 0;
        }
    }
}

std::chrono::system_clock::time_point fromTm(const std::tm& tm) {
    auto localTm = tm;
    return std::chrono::system_clock::from_time_t(std::mktime(&localTm));
}

std::optional<std::chrono::system_clock::time_point> parseDateTime(const mysqlx::Value& value) {
    if (value.isNull()) {
        return std::nullopt;
    }
    try {
        auto text = sanitizeDateTimeString(valueToString(value));
        if (text.empty()) {
            return std::nullopt;
        }
        std::tm tm{};
        std::istringstream iss(text);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (iss.fail()) {
            util::log(util::LogLevel::warn, std::string{"解析日期列失败: "} + text);
            return std::nullopt;
        }
        return fromTm(tm);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"读取日期列失败: "} + ex.what());
        return std::nullopt;
    }
}

model::Buyer mapBuyer(mysqlx::Row row) {
    model::Buyer buyer{};
    std::size_t index = 0;
    buyer.id = readInt(row[index++]);
    buyer.username = readString(row[index++]);
    buyer.password = readString(row[index++]);
    buyer.email = readString(row[index++]);
    buyer.accessLevel = readInt(row[index++]);
    buyer.dailyMaxSubmissions = readInt(row[index++]);
    buyer.dailySubmissionCount = readInt(row[index++]);
    buyer.validityPeriod = parseDateTime(row[index++]);
    return buyer;
}

constexpr const char* kSelectColumns[] = {
    "id",
    "username",
    "password",
    "email",
    "access_level",
    "daily_max_submissions",
    "daily_submission_count",
    "validity_period"};

} // namespace

BuyersRepository::BuyersRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

std::vector<model::Buyer> BuyersRepository::findAll() {
    std::vector<model::Buyer> buyers;
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("buyers");
        mysqlx::TableSelect select = table.select(kSelectColumns[0],
                                                  kSelectColumns[1],
                                                  kSelectColumns[2],
                                                  kSelectColumns[3],
                                                  kSelectColumns[4],
                                                  kSelectColumns[5],
                                                  kSelectColumns[6],
                                                  kSelectColumns[7]);
        mysqlx::RowResult rows = select.execute();
        buyers.reserve(rows.count());
        for (mysqlx::Row row : rows) {
            auto buyer = mapBuyer(std::move(row));
            buyer.password.clear();
            buyers.emplace_back(std::move(buyer));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"查询买家列表失败: "} + err.what());
        throw;
    }
    return buyers;
}

std::optional<model::Buyer> BuyersRepository::findByUsername(const std::string& username) {
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("buyers");
        mysqlx::RowResult rows = table
                                     .select(kSelectColumns[0],
                                             kSelectColumns[1],
                                             kSelectColumns[2],
                                             kSelectColumns[3],
                                             kSelectColumns[4],
                                             kSelectColumns[5],
                                             kSelectColumns[6],
                                             kSelectColumns[7])
                                     .where("username = :username")
                                     .bind("username", username)
                                     .limit(1)
                                     .execute();

        for (mysqlx::Row row : rows) {
            return mapBuyer(std::move(row));
        }
        return std::nullopt;
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"按用户名查询买家失败: "} + err.what());
        throw;
    }
}

} // namespace quickgrab::repository

