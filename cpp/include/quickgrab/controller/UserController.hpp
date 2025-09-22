#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"

namespace quickgrab::controller {

class UserController {
public:
    explicit UserController(service::AuthService& authService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleGetUser(quickgrab::server::RequestContext& ctx);

    service::AuthService& authService_;
};

} // namespace quickgrab::controller
