#include "quickgrab/repository/BuyersRepository.hpp"
#include "quickgrab/util/Logging.hpp"

#include <mysqlx/xdevapi.h>

#include <exception>

namespace quickgrab::repository {

BuyersRepository::BuyersRepository(MySqlConnectionPool& pool)
    : pool_(pool) {}

std::vector<model::Buyer> BuyersRepository::findAll() {
    std::vector<model::Buyer> buyers;
    auto session = pool_.acquire();
    try {
        mysqlx::Schema schema = session->getSchema(pool_.schemaName());
        mysqlx::Table table = schema.getTable("buyers");
        mysqlx::RowResult rows = table.select("id", "username").execute();
        buyers.reserve(rows.count());
        for (mysqlx::Row row : rows) {
            model::Buyer buyer{};
            try {
                buyer.id = row[0].get<int>();
            } catch (const std::exception& ex) {
                util::log(util::LogLevel::warn, std::string{"读取买家 ID 失败: "} + ex.what());
                continue;
            }
            try {
                if (!row[1].isNull()) {
                    buyer.username = row[1].get<std::string>();
                }
            } catch (const std::exception& ex) {
                util::log(util::LogLevel::warn, std::string{"读取买家用户名失败: "} + ex.what());
            }
            buyers.emplace_back(std::move(buyer));
        }
    } catch (const mysqlx::Error& err) {
        util::log(util::LogLevel::error, std::string{"查询买家列表失败: "} + err.what());
        throw;
    }
    return buyers;
}

} // namespace quickgrab::repository

