#pragma once

#include "quickgrab/server/Router.hpp"
#include "quickgrab/service/AuthService.hpp"

namespace quickgrab::controller {

class AuthController {
public:
    explicit AuthController(service::AuthService& authService);

    void registerRoutes(quickgrab::server::Router& router);

private:
    enum class SessionResponseMode {
        full,
        probe,
    };

    void handleLogin(quickgrab::server::RequestContext& ctx);
    void handleLogout(quickgrab::server::RequestContext& ctx);
    void handleSessionStatus(quickgrab::server::RequestContext& ctx, SessionResponseMode mode);

    service::AuthService& authService_;
};

} // namespace quickgrab::controller
