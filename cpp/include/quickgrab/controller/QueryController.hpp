#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/QueryService.hpp"

namespace quickgrab::controller {

class QueryController {
public:
    explicit QueryController(service::QueryService& queryService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handlePending(quickgrab::server::RequestContext& ctx);
    void handleGetRequests(quickgrab::server::RequestContext& ctx);
    void handleGetResults(quickgrab::server::RequestContext& ctx);
    void handleDeleteRequest(quickgrab::server::RequestContext& ctx, int requestId);
    void handleDeleteResult(quickgrab::server::RequestContext& ctx, int resultId);
    void handleGetResult(quickgrab::server::RequestContext& ctx, int resultId);
    void handleGetBuyers(quickgrab::server::RequestContext& ctx);

    service::QueryService& queryService_;
};

} // namespace quickgrab::controller
