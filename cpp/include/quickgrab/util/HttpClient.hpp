#pragma once

#include "quickgrab/proxy/ProxyPool.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace quickgrab::util {

class HttpClient {
public:
    using HttpRequest = boost::beast::http::request<boost::beast::http::string_body>;
    using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;

    struct Header {
        std::string name;
        std::string value;
    };

    HttpClient(boost::asio::io_context& io, proxy::ProxyPool& pool);

    HttpResponse fetch(HttpRequest request,
                       const std::string& affinityKey,
                       std::chrono::seconds timeout,
                       bool useProxy = false,
                       const proxy::ProxyEndpoint* overrideProxy = nullptr);

    HttpResponse fetch(const std::string& method,
                       const std::string& url,
                       const std::vector<Header>& headers,
                       const std::string& body,
                       const std::string& affinityKey,
                       std::chrono::seconds timeout,
                       bool followRedirects = false,
                       unsigned int maxRedirects = 5,
                       std::string* effectiveUrl = nullptr,
                       bool useProxy = false,
                       const proxy::ProxyEndpoint* overrideProxy = nullptr);

private:
    boost::asio::io_context& io_;
    proxy::ProxyPool& proxyPool_;
    boost::asio::ssl::context sslContext_;
};

} // namespace quickgrab::util

