#pragma once

#include <boost/json.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace quickgrab::util {

inline std::string formatIsoTimestamp(std::chrono::system_clock::time_point timePoint) {
    const std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

inline std::string makeIsoTimestamp() {
    return formatIsoTimestamp(std::chrono::system_clock::now());
}

inline boost::json::object makeSuccessResponse(const boost::json::value& data,
                                               std::string_view endpoint,
                                               std::string_view message = {}) {
    boost::json::object envelope;
    envelope["success"] = true;
    envelope["timestamp"] = makeIsoTimestamp();
    if (!endpoint.empty()) {
        envelope["path"] = endpoint;
    }
    if (!message.empty()) {
        envelope["message"] = message;
    }
    envelope["data"] = data;
    return envelope;
}

inline boost::json::object makeErrorDetails(std::string_view message,
                                            const boost::json::value& details,
                                            std::string_view endpoint) {
    boost::json::object errorObj;
    if (!message.empty()) {
        errorObj["message"] = message;
    }
    if (!details.is_null()) {
        errorObj["details"] = details;
    }

    boost::json::object envelope;
    envelope["success"] = false;
    envelope["timestamp"] = makeIsoTimestamp();
    if (!endpoint.empty()) {
        envelope["path"] = endpoint;
    }
    envelope["error"] = std::move(errorObj);
    return envelope;
}

} // namespace quickgrab::util

