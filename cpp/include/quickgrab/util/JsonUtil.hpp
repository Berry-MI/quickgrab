#pragma once

#include <boost/json.hpp>
#include <string>

namespace quickgrab::util {

boost::json::value parseJson(const std::string& payload);
std::string stringifyJson(const boost::json::value& value);

} // namespace quickgrab::util
