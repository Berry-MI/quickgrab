#pragma once

#include <boost/beast/http.hpp>
#include <chrono>
#include <string>
#include <unordered_map>

namespace quickgrab::server {

struct RequestContext {
    using HttpRequest = boost::beast::http::request<boost::beast::http::string_body>;
    using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;

    HttpRequest request;
    HttpResponse response;
    std::unordered_map<std::string, std::string> pathParameters;
    std::chrono::steady_clock::time_point startedAt;
};

} // namespace quickgrab::server
