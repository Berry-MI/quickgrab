#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/util/HttpClient.hpp"

namespace quickgrab::controller {

class ProxyController {
public:
    explicit ProxyController(util::HttpClient& client);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleUpload(quickgrab::server::RequestContext& ctx);
    void handleExpand(quickgrab::server::RequestContext& ctx);
    void handleGetItemSkuInfo(quickgrab::server::RequestContext& ctx);
    void handleLoginByVcode(quickgrab::server::RequestContext& ctx);
    void handleGetListCart(quickgrab::server::RequestContext& ctx);
    void handleGetUserInfo(quickgrab::server::RequestContext& ctx);
    void handleGetAddOrderData(quickgrab::server::RequestContext& ctx);
    void handleProxyRequest(quickgrab::server::RequestContext& ctx);

    util::HttpClient& httpClient_;
};

} // namespace quickgrab::controller
