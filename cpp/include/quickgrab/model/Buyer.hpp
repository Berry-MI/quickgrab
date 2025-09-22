#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace quickgrab::model {

struct Buyer {
    int id{};
    std::string username;
    std::string password;
    std::string email;
    int accessLevel{};
    int dailyMaxSubmissions{};
    int dailySubmissionCount{};
    std::optional<std::chrono::system_clock::time_point> validityPeriod;
};

} // namespace quickgrab::model

