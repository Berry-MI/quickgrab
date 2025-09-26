#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"
#include "quickgrab/service/GrabService.hpp"

namespace quickgrab::controller {

class SubmitController {
public:
    SubmitController(service::GrabService& grabService, service::AuthService& authService);
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleSubmitRequest(quickgrab::server::RequestContext& ctx);

    service::GrabService& grabService_;
    service::AuthService& authService_;
};

} // namespace quickgrab::controller
