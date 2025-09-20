#include \"quickgrab/controller/QueryController.hpp\"
#include \"quickgrab/util/JsonUtil.hpp\"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

namespace quickgrab::controller {

QueryController::QueryController(service::QueryService& queryService)
    : queryService_(queryService) {}

void QueryController::registerRoutes(quickgrab::server::Router& router) {
    router.addRoute("GET", "/api/grab/pending", [this](auto& ctx) { handlePending(ctx); });
}

void QueryController::handlePending(quickgrab::server::RequestContext& ctx) {
    auto pending = queryService_.listPending(20);
    boost::json::array payload;
    for (const auto& request : pending) {
        payload.emplace_back(boost::json::object{{"id", request.id}, {"link", request.link}});
    }
    ctx.response.result(boost::beast::http::status::ok);
    ctx.response.set(boost::beast::http::field::content_type, "application/json");
    ctx.response.body() = quickgrab::util::stringifyJson(payload);
    ctx.response.prepare_payload();
}

} // namespace quickgrab::controller
