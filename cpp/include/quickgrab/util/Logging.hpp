#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace quickgrab::util {

enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error
};

void initLogging(LogLevel level);
void log(LogLevel level, const std::string& message);

} // namespace quickgrab::util
