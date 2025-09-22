#pragma once

#include "quickgrab/repository/BuyersRepository.hpp"
#include "quickgrab/repository/ResultsRepository.hpp"

#include <optional>
#include <string>
#include <vector>

namespace quickgrab::service {

class StatisticsService {
public:
    StatisticsService(repository::ResultsRepository& results,
                      repository::BuyersRepository& buyers);

    std::vector<repository::ResultsRepository::AggregatedStats> getStatistics(
        const std::optional<int>& buyerId,
        const std::optional<std::string>& startTime,
        const std::optional<std::string>& endTime);

    std::vector<repository::ResultsRepository::DailyStat> getDailyStats(const std::optional<int>& buyerId,
                                                                        const std::optional<int>& status);

    std::vector<repository::ResultsRepository::HourlyStat> getHourlyStats(const std::optional<int>& buyerId,
                                                                         const std::optional<int>& status);

    std::vector<model::Buyer> getAllBuyers();

private:
    repository::ResultsRepository& results_;
    repository::BuyersRepository& buyers_;
};

} // namespace quickgrab::service

