#include \"quickgrab/controller/ToolController.hpp\"

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

boost::json::object makeNotImplemented() {
    return { { "error", "not_implemented" } };
}
}

void ToolController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/getNote", [this](auto& ctx) { handleGetNote(ctx); });
    router.addRoute("POST", "/fetchItemInfo", [this](auto& ctx) { handleFetchItemInfo(ctx); });
    router.addRoute("GET", "/checkCookiesValidity", [this](auto& ctx) { handleCheckCookies(ctx); });
    router.addRoute("POST", "/checkLatency", [this](auto& ctx) { handleCheckLatency(ctx); });
}

void ToolController::handleGetNote(quickgrab::server::RequestContext& ctx) {
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, makeNotImplemented());
}

void ToolController::handleFetchItemInfo(quickgrab::server::RequestContext& ctx) {
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, makeNotImplemented());
}

void ToolController::handleCheckCookies(quickgrab::server::RequestContext& ctx) {
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, makeNotImplemented());
}

void ToolController::handleCheckLatency(quickgrab::server::RequestContext& ctx) {
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, makeNotImplemented());
}

} // namespace quickgrab::controller
