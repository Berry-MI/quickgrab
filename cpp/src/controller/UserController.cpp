#include \"quickgrab/controller/UserController.hpp\"

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

void UserController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/user", [this](auto& ctx) { handleGetUser(ctx); });
}

void UserController::handleGetUser(quickgrab::server::RequestContext& ctx) {
    boost::json::object payload{
        {"error", "not_implemented"}
    };
    sendJsonResponse(ctx, boost::beast::http::status::not_implemented, payload);
}

} // namespace quickgrab::controller
