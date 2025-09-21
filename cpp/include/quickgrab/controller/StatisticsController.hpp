#pragma once

#include "quickgrab/server/Router.hpp"

namespace quickgrab::controller {

class StatisticsController {
public:
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleStatistics(quickgrab::server::RequestContext& ctx);
    void handleDailyStats(quickgrab::server::RequestContext& ctx);
    void handleHourlyStats(quickgrab::server::RequestContext& ctx);
    void handleBuyers(quickgrab::server::RequestContext& ctx);
};

} // namespace quickgrab::controller
