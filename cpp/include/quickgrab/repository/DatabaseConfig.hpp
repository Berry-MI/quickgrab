#pragma once

#include <boost/json.hpp>
#include <string>

namespace quickgrab::repository {

struct DatabaseConfig {
    std::string host;
    std::uint16_t port{33060};
    std::string user;
    std::string password;
    std::string database;
    std::string charset{"utf8mb4"};
    unsigned int poolSize{8};
};

DatabaseConfig loadConfig(const boost::json::object& json);

} // namespace quickgrab::repository

