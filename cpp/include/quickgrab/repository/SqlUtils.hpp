#pragma once

#include <algorithm>
#include <sstream>
#include <string>

namespace quickgrab::repository {

inline std::string buildLimitOffsetClause(int limit, int offset) {
    const int safeLimit = std::max(0, limit);
    const int safeOffset = std::max(0, offset);

    std::ostringstream oss;
    oss << " LIMIT " << safeLimit << " OFFSET " << safeOffset;
    return oss.str();
}

} // namespace quickgrab::repository

