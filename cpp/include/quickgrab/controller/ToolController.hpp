#pragma once

#include "quickgrab/server/Router.hpp"

namespace quickgrab::util {
class HttpClient;
}

namespace quickgrab::controller {

class ToolController {
public:
    explicit ToolController(quickgrab::util::HttpClient& httpClient);
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleGetNote(quickgrab::server::RequestContext& ctx);
    void handleFetchItemInfo(quickgrab::server::RequestContext& ctx);
    void handleCheckCookies(quickgrab::server::RequestContext& ctx);
    void handleCheckLatency(quickgrab::server::RequestContext& ctx);

    quickgrab::util::HttpClient& httpClient_;
};

} // namespace quickgrab::controller
