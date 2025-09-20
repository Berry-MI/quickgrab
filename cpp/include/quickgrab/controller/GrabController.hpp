#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/GrabService.hpp"

namespace quickgrab::controller {

class GrabController {
public:
    explicit GrabController(service::GrabService& grabService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleRun(quickgrab::server::RequestContext& ctx);

    service::GrabService& grabService_;
};

} // namespace quickgrab::controller
