#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"
#include "quickgrab/service/GrabService.hpp"
#include "quickgrab/util/HttpClient.hpp"

namespace quickgrab::controller {

class SubmitController {
public:
    SubmitController(service::GrabService& grabService,
                     service::AuthService& authService,
                     util::HttpClient& httpClient);
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleSubmitRequest(quickgrab::server::RequestContext& ctx);

    service::GrabService& grabService_;
    service::AuthService& authService_;
    util::HttpClient& httpClient_;
};

} // namespace quickgrab::controller
