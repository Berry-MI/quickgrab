#pragma once

#include <boost/json/fwd.hpp>
#include <optional>
#include <string>

namespace quickgrab::util {

std::optional<boost::json::value> extractDataObject(const std::string& html);

} // namespace quickgrab::util
