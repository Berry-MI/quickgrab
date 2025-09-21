#include "quickgrab/controller/GrabController.hpp"
#include "quickgrab/util/JsonUtil.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

namespace quickgrab::controller {

GrabController::GrabController(service::GrabService& grabService)
    : grabService_(grabService) {}

void GrabController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("POST", "/api/grab/run", [this](auto& ctx) { handleRun(ctx); });
}

void GrabController::handleRun(quickgrab::server::RequestContext& ctx) {
    grabService_.processPending();
    boost::json::object payload{{"status", "queued"}};
    ctx.response.result(boost::beast::http::status::ok);
    ctx.response.set(boost::beast::http::field::content_type, "application/json");
    ctx.response.body() = quickgrab::util::stringifyJson(payload);
    ctx.response.prepare_payload();
}

} // namespace quickgrab::controller
