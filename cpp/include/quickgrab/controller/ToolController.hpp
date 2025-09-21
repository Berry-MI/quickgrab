#pragma once

#include "quickgrab/server/Router.hpp"

namespace quickgrab::controller {

class ToolController {
public:
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleGetNote(quickgrab::server::RequestContext& ctx);
    void handleFetchItemInfo(quickgrab::server::RequestContext& ctx);
    void handleCheckCookies(quickgrab::server::RequestContext& ctx);
    void handleCheckLatency(quickgrab::server::RequestContext& ctx);
};

} // namespace quickgrab::controller
