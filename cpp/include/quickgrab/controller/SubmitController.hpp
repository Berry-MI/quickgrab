#pragma once

#include "quickgrab/server/Router.hpp"

namespace quickgrab::controller {

class SubmitController {
public:
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleSubmitRequest(quickgrab::server::RequestContext& ctx);
};

} // namespace quickgrab::controller
