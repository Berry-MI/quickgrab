#include "quickgrab/controller/AuthController.hpp"

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
}

void AuthController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/logout", [this](auto& ctx) { handleLogout(ctx); });
}

void AuthController::handleLogout(quickgrab::server::RequestContext& ctx) {
    boost::json::object payload{{"status", "success"}, {"message", "Logout successful"}};
    sendJsonResponse(ctx, boost::beast::http::status::ok, payload);
}

} // namespace quickgrab::controller
