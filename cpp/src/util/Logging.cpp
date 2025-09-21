#include "quickgrab/util/Logging.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace quickgrab::util {
namespace {
std::mutex& logMutex() {
    static std::mutex m;
    return m;
}

LogLevel& globalLevel() {
    static LogLevel level = LogLevel::info;
    return level;
}

const char* toString(LogLevel level) {
    switch (level) {
    case LogLevel::trace: return "TRACE";
    case LogLevel::debug: return "DEBUG";
    case LogLevel::info:  return "INFO";
    case LogLevel::warn:  return "WARN";
    case LogLevel::error: return "ERROR";
    }
    return "INFO";
}

bool shouldLog(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(globalLevel());
}
}

void initLogging(LogLevel level) {
    globalLevel() = level;
}

void log(LogLevel level, const std::string& message) {
    if (!shouldLog(level)) {
        return;
    }
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &time);
#else
    localtime_r(&time, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");

    std::lock_guard lock(logMutex());
    std::clog << oss.str() << " [" << toString(level) << "] " << message << std::endl;
}

} // namespace quickgrab::util
