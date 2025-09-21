#pragma once

#include <boost/json.hpp>
#include <chrono>
#include <optional>
#include <string>

namespace quickgrab::model {

struct Result {
    int id{};
    int requestId{};
    int deviceId{};
    int buyerId{};
    std::string threadId;
    std::string link;
    std::string cookies;
    boost::json::value orderInfo;
    boost::json::value userInfo;
    boost::json::value orderTemplate;
    std::string message;
    std::string idNumber;
    std::string keyword;
    std::chrono::system_clock::time_point startTime{};
    std::chrono::system_clock::time_point endTime{};
    int quantity{};
    int delay{};
    int frequency{};
    int type{};
    int status{};
    boost::json::value responseMessage;
    double actualEarnings{};
    double estimatedEarnings{};
    boost::json::value extension;
    boost::json::value payload;
    std::chrono::system_clock::time_point createdAt{};
};

} // namespace quickgrab::model
