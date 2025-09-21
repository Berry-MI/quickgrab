#include "quickgrab/util/JsonUtil.hpp"

namespace quickgrab::util {

boost::json::value parseJson(const std::string& payload) {
    return boost::json::parse(payload);
}

std::string stringifyJson(const boost::json::value& value) {
    return boost::json::serialize(value);
}

} // namespace quickgrab::util
