#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/GrabService.hpp"

namespace quickgrab::controller {

class SubmitController {
public:
    explicit SubmitController(service::GrabService& grabService);
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleSubmitRequest(quickgrab::server::RequestContext& ctx);

    service::GrabService& grabService_;
};

} // namespace quickgrab::controller
