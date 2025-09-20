#pragma once

#include <boost/json.hpp>
#include <chrono>
#include <optional>
#include <string>

namespace quickgrab::model {

struct Result {
    int id{};
    int requestId{};
    std::string status;
    boost::json::value payload;
    std::chrono::system_clock::time_point createdAt{};
};

} // namespace quickgrab::model
