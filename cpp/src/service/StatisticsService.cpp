#include "quickgrab/service/StatisticsService.hpp"

namespace quickgrab::service {

StatisticsService::StatisticsService(repository::ResultsRepository& results,
                                     repository::BuyersRepository& buyers)
    : results_(results)
    , buyers_(buyers) {}

std::vector<repository::ResultsRepository::AggregatedStats> StatisticsService::getStatistics(
    const std::optional<int>& buyerId,
    const std::optional<std::string>& startTime,
    const std::optional<std::string>& endTime) {
    return results_.getStatistics(buyerId, startTime, endTime);
}

std::vector<repository::ResultsRepository::DailyStat> StatisticsService::getDailyStats(
    const std::optional<int>& buyerId,
    const std::optional<int>& status) {
    return results_.getDailyStats(buyerId, status);
}

std::vector<repository::ResultsRepository::HourlyStat> StatisticsService::getHourlyStats(
    const std::optional<int>& buyerId,
    const std::optional<int>& status) {
    return results_.getHourlyStats(buyerId, status);
}

std::vector<model::Buyer> StatisticsService::getAllBuyers() {
    return buyers_.findAll();
}

} // namespace quickgrab::service

