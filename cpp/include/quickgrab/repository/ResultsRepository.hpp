#pragma once

#include "quickgrab/model/Result.hpp"
#include "quickgrab/repository/MySqlConnectionPool.hpp"

#include <boost/json.hpp>
#include <mysqlx/xdevapi.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quickgrab::repository {

class ResultsRepository {
public:
    explicit ResultsRepository(MySqlConnectionPool& pool);

    void insertResult(const model::Result& result);
    std::optional<model::Result> findById(int resultId);
    void deleteById(int resultId);
    std::vector<model::Result> findByFilters(const std::optional<std::string>& keyword,
                                             const std::optional<int>& buyerId,
                                             const std::optional<int>& type,
                                             const std::optional<int>& status,
                                             std::string_view orderColumn,
                                             std::string_view orderDirection,
                                             int offset,
                                             int limit);

    struct AggregatedStats {
        std::string type;
        std::int64_t successCount{};
        std::int64_t failureCount{};
        std::int64_t exceptionCount{};
        std::int64_t totalCount{};
        double successEarnings{};
        double failureEarnings{};
        double exceptionEarnings{};
        double totalEarnings{};
    };

    struct DailyStat {
        std::string date;
        std::int64_t total{};
        double earnings{};
    };

    struct HourlyStat {
        int hour{};
        std::int64_t total{};
        double earnings{};
    };

    std::vector<AggregatedStats> getStatistics(const std::optional<int>& buyerId,
                                               const std::optional<std::string>& startTime,
                                               const std::optional<std::string>& endTime);
    std::vector<DailyStat> getDailyStats(const std::optional<int>& buyerId,
                                         const std::optional<int>& status);
    std::vector<HourlyStat> getHourlyStats(const std::optional<int>& buyerId,
                                           const std::optional<int>& status);

private:
    model::Result mapRow(mysqlx::Row row);
    model::Result mapDetailedRow(mysqlx::Row row);
    std::chrono::system_clock::time_point parseTimestamp(const std::string& value);

    MySqlConnectionPool& pool_;
};

} // namespace quickgrab::repository
