#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/StatisticsService.hpp"

namespace quickgrab::controller {

class StatisticsController {
public:
    explicit StatisticsController(service::StatisticsService& statisticsService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleStatistics(quickgrab::server::RequestContext& ctx);
    void handleDailyStats(quickgrab::server::RequestContext& ctx);
    void handleHourlyStats(quickgrab::server::RequestContext& ctx);
    void handleBuyers(quickgrab::server::RequestContext& ctx);

    service::StatisticsService& statisticsService_;
};

} // namespace quickgrab::controller
