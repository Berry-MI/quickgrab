#pragma once

#include "quickgrab/server/Router.hpp"

namespace quickgrab {
namespace util {
class HttpClient;
} // namespace util

namespace controller {

class ToolController {
public:
    explicit ToolController(util::HttpClient& httpClient);
    void registerRoutes(server::Router& router);

private:
    void handleGetNote(server::RequestContext& ctx);
    void handleFetchItemInfo(server::RequestContext& ctx);
    void handleCheckCookies(server::RequestContext& ctx);
    void handleCheckLatency(server::RequestContext& ctx);

    util::HttpClient& httpClient_;
};

} // namespace controller
} // namespace quickgrab
