#include "quickgrab/controller/StatisticsController.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

namespace quickgrab::controller {
namespace {
void sendJsonResponse(quickgrab::server::RequestContext& ctx,
                      boost::beast::http::status status,
                      const boost::json::value& body) {
    ctx.response.result(status);
    ctx.response.set(boost::beast::http::field::content_type, "application/json; charset=utf-8");
    ctx.response.body() = boost::json::serialize(body);
    ctx.response.prepare_payload();
}

void sendNotImplemented(quickgrab::server::RequestContext& ctx) {
    boost::json::object payload{{"error", "not_implemented"}};
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, payload);
}
}

void StatisticsController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/statistics", [this](auto& ctx) { handleStatistics(ctx); });
    router.addRoute("GET", "/api/dailyStats", [this](auto& ctx) { handleDailyStats(ctx); });
    router.addRoute("GET", "/api/hourlyStats", [this](auto& ctx) { handleHourlyStats(ctx); });
    router.addRoute("GET", "/api/buyers", [this](auto& ctx) { handleBuyers(ctx); });
}

void StatisticsController::handleStatistics(quickgrab::server::RequestContext& ctx) {
    sendNotImplemented(ctx);
}

void StatisticsController::handleDailyStats(quickgrab::server::RequestContext& ctx) {
    sendNotImplemented(ctx);
}

void StatisticsController::handleHourlyStats(quickgrab::server::RequestContext& ctx) {
    sendNotImplemented(ctx);
}

void StatisticsController::handleBuyers(quickgrab::server::RequestContext& ctx) {
    sendNotImplemented(ctx);
}

} // namespace quickgrab::controller
