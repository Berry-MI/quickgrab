#include "quickgrab/repository/DatabaseConfig.hpp"

namespace quickgrab::repository {

DatabaseConfig loadConfig(const boost::json::object& json) {
    DatabaseConfig cfg;
    if (auto it = json.if_contains("host")) cfg.host = std::string(it->as_string());
    if (auto it = json.if_contains("port")) cfg.port = static_cast<std::uint16_t>(it->as_int64());
    if (auto it = json.if_contains("user")) cfg.user = std::string(it->as_string());
    if (auto it = json.if_contains("password")) cfg.password = std::string(it->as_string());
    if (auto it = json.if_contains("database")) cfg.database = std::string(it->as_string());
    if (auto it = json.if_contains("charset")) cfg.charset = std::string(it->as_string());
    if (auto it = json.if_contains("poolSize")) cfg.poolSize = static_cast<unsigned int>(it->as_int64());
    return cfg;
}

} // namespace quickgrab::repository
