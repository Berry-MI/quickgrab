#pragma once

#include \"quickgrab/server/Router.hpp\"

namespace quickgrab::controller {

class AuthController {
public:
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleLogout(quickgrab::server::RequestContext& ctx);
};

} // namespace quickgrab::controller
