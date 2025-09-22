#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"

namespace quickgrab::controller {

class AuthController {
public:
    explicit AuthController(service::AuthService& authService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleLogin(quickgrab::server::RequestContext& ctx);
    void handleLogout(quickgrab::server::RequestContext& ctx);

    service::AuthService& authService_;
};

} // namespace quickgrab::controller
