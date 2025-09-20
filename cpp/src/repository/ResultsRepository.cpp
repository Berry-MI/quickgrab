#include \"quickgrab/repository/ResultsRepository.hpp\"
#include \"quickgrab/util/JsonUtil.hpp\"
#include \"quickgrab/util/Logging.hpp\"

#include <cppconn/prepared_statement.h>

#include <chrono>
#include <iomanip>
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
}

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

} // namespace quickgrab::repository




