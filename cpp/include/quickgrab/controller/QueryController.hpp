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

    service::QueryService& queryService_;
};

} // namespace quickgrab::controller
