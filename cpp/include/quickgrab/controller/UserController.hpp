#pragma once

#include \"quickgrab/server/Router.hpp\"

namespace quickgrab::controller {

class UserController {
public:
    void registerRoutes(quickgrab::server::Router& router);

private:
    void handleGetUser(quickgrab::server::RequestContext& ctx);
};

} // namespace quickgrab::controller
