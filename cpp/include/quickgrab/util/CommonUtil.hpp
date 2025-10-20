#pragma once

#include "quickgrab/model/Request.hpp"

#include <boost/json.hpp>

#include <optional>

namespace quickgrab::util {

class HttpClient;

// 构建下单所需的参数结构，尽量与 Java 版本 CommonUtil 保持一致。
std::optional<boost::json::object> generateOrderParameters(const model::Request& request,
                                                           const boost::json::object& dataObj,
                                                           bool includeInvalid,
                                                           HttpClient* httpClient = nullptr);

} // namespace quickgrab::util

