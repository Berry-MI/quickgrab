#include "quickgrab/util/Logging.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <format> // C++20
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
    if (!shouldLog(level)) return;

    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto sec_tp = floor<seconds>(now);
    const auto ms = duration_cast<milliseconds>(now - sec_tp).count();

    std::time_t t = system_clock::to_time_t(sec_tp);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms;

    std::lock_guard lk(logMutex());
    std::clog << oss.str() << " [" << toString(level) << "] " << message << '\n';
}


} // namespace quickgrab::util
