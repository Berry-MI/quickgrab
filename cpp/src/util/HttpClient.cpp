#include "quickgrab/util/HttpClient.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>
#include <boost/beast/version.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>


namespace quickgrab::util {
namespace {
constexpr unsigned kHttpVersion = 11;

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl parsed;
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        throw std::invalid_argument("URL missing scheme: " + url);
    }
    parsed.scheme = url.substr(0, schemeEnd);
    std::transform(parsed.scheme.begin(), parsed.scheme.end(), parsed.scheme.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    auto hostStart = schemeEnd + 3;
    auto pathPos = url.find('/', hostStart);
    std::string hostPort = pathPos == std::string::npos ? url.substr(hostStart) : url.substr(hostStart, pathPos - hostStart);
    auto colonPos = hostPort.find(':');
    if (colonPos == std::string::npos) {
        parsed.host = hostPort;
        parsed.port = (parsed.scheme == "https") ? "443" : "80";
    } else {
        parsed.host = hostPort.substr(0, colonPos);
        parsed.port = hostPort.substr(colonPos + 1);
    }
    parsed.target = pathPos == std::string::npos ? "/" : url.substr(pathPos);
    if (parsed.target.empty()) {
        parsed.target = "/";
    }
    return parsed;
}

bool isRedirect(boost::beast::http::status status) {
    switch (status) {
    case boost::beast::http::status::moved_permanently:
    case boost::beast::http::status::found:
    case boost::beast::http::status::see_other:
    case boost::beast::http::status::temporary_redirect:
    case boost::beast::http::status::permanent_redirect:
        return true;
    default:
        return false;
    }
}

std::string combineLocation(const ParsedUrl& base, const std::string& location) {
    if (location.empty()) {
        return base.scheme + "://" + base.host + base.target;
    }
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
        return location;
    }
    std::string prefix = base.scheme + "://" + base.host;
    if (!base.port.empty() && base.port != "80" && base.port != "443") {
        prefix += ":" + base.port;
    }
    if (location.front() == '/') {
        return prefix + location;
    }
    auto slashPos = base.target.find_last_of('/');
    std::string basePath = slashPos == std::string::npos ? "/" : base.target.substr(0, slashPos + 1);
    return prefix + basePath + location;
}

boost::beast::http::verb toVerb(const std::string& method) {
    std::string upper;
    upper.reserve(method.size());
    std::transform(method.begin(), method.end(), std::back_inserter(upper), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    if (upper == "GET") return boost::beast::http::verb::get;
    if (upper == "POST") return boost::beast::http::verb::post;
    if (upper == "PUT") return boost::beast::http::verb::put;
    if (upper == "DELETE") return boost::beast::http::verb::delete_;
    if (upper == "PATCH") return boost::beast::http::verb::patch;
    if (upper == "HEAD") return boost::beast::http::verb::head;
    if (upper == "OPTIONS") return boost::beast::http::verb::options;
    if (upper == "TRACE") return boost::beast::http::verb::trace;
    throw std::invalid_argument("Unsupported HTTP method: " + method);
}

std::string base64Encode(std::string_view input) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    std::uint32_t value = 0;
    int bitCount = -6;
    for (unsigned char c : input) {
        value = (value << 8) | c;
        bitCount += 8;
        while (bitCount >= 0) {
            output.push_back(alphabet[(value >> bitCount) & 0x3F]);
            bitCount -= 6;
        }
    }

    if (bitCount > -6) {
        output.push_back(alphabet[((value << 8) >> (bitCount + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }
    return output;
}

std::string proxyAuthorization(const proxy::ProxyEndpoint& proxy) {
    if (proxy.username.empty() && proxy.password.empty()) {
        return {};
    }
    std::string credentials = proxy.username + ":" + proxy.password;
    return "Basic " + base64Encode(credentials);
}

std::string authorityFrom(const ParsedUrl& parsed) {
    if ((parsed.scheme == "http" && parsed.port == "80") ||
        (parsed.scheme == "https" && parsed.port == "443")) {
        return parsed.host;
    }
    return parsed.host + ":" + parsed.port;
}

} // namespace

HttpClient::HttpClient(boost::asio::io_context& io, proxy::ProxyPool& pool)
    : io_(io)
    , proxyPool_(pool)
    , sslContext_(boost::asio::ssl::context::tls_client) {
    sslContext_.set_default_verify_paths();
    sslContext_.set_verify_mode(boost::asio::ssl::verify_none);
}

HttpClient::HttpResponse HttpClient::fetch(HttpRequest request,
                                           const std::string& affinityKey,
                                           std::chrono::seconds timeout,
                                           bool useProxy)
{
    request.version(kHttpVersion);
    if (!request.has(boost::beast::http::field::host)) {
        throw std::runtime_error("request missing Host header");
    }

    std::string scheme = "http";
    if (auto it = request.find("X-Quick-Scheme"); it != request.end()) {
        scheme = std::string(it->value());
        request.erase(it);
    }

    ParsedUrl parsed;
    parsed.scheme = scheme;
    parsed.host = std::string(request[boost::beast::http::field::host]);
    parsed.port = (scheme == "https") ? "443" : "80";
    auto colon = parsed.host.find(':');
    if (colon != std::string::npos) {
        parsed.port = parsed.host.substr(colon + 1);
        parsed.host = parsed.host.substr(0, colon);
    }
    parsed.target = std::string(request.target());
    if (parsed.target.empty()) {
        parsed.target = "/";
    }

    request.erase(boost::beast::http::field::host);
    request.set(boost::beast::http::field::host, parsed.host);

    std::optional<proxy::ProxyEndpoint> proxy;
    if (useProxy) {
        if (affinityKey.empty()) {
            util::log(util::LogLevel::warn, "Proxy requested but affinity key is empty; sending directly");
        } else {
            proxy = proxyPool_.acquire(affinityKey);
            if (!proxy) {
                util::log(util::LogLevel::warn, "No proxy available for affinity key " + affinityKey);
            }
        }
    }
    try {
        if (proxy) {
            boost::asio::ip::tcp::resolver resolver(io_);
            auto proxyResults = resolver.resolve(proxy->host, std::to_string(proxy->port));

            if (parsed.scheme == "https") {
                boost::beast::ssl_stream<boost::beast::tcp_stream> stream(io_, sslContext_);
                auto& lowest = boost::beast::get_lowest_layer(stream);
                lowest.expires_after(timeout);
                lowest.connect(proxyResults);

                boost::beast::http::request<boost::beast::http::empty_body> connectRequest{
                    boost::beast::http::verb::connect, authorityFrom(parsed), kHttpVersion};
                connectRequest.set(boost::beast::http::field::host, authorityFrom(parsed));
                if (auto auth = proxyAuthorization(*proxy); !auth.empty()) {
                    connectRequest.set("Proxy-Authorization", auth);
                }

                boost::beast::http::write(lowest, connectRequest);
                boost::beast::flat_buffer connectBuffer;
                boost::beast::http::response<boost::beast::http::empty_body> connectResponse;
                boost::beast::http::read(lowest, connectBuffer, connectResponse);
                if (connectResponse.result() != boost::beast::http::status::ok) {
                    throw std::runtime_error("Proxy CONNECT failed with status " +
                                             std::to_string(connectResponse.result_int()));
                }

                if (!SSL_set_tlsext_host_name(stream.native_handle(), parsed.host.c_str())) {
                    throw std::runtime_error("Failed to set SNI host name");
                }

                lowest.expires_after(timeout);
                stream.handshake(boost::asio::ssl::stream_base::client);

                boost::beast::http::write(stream, request);
                boost::beast::flat_buffer buffer;
                HttpResponse response;
                boost::beast::http::read(stream, buffer, response);

                boost::system::error_code ec;
                stream.shutdown(ec);
                if (ec == boost::asio::error::eof || ec == boost::asio::ssl::error::stream_truncated) {
                    ec = {};
                }
                if (ec) {
                    throw boost::system::system_error(ec);
                }


                proxyPool_.reportSuccess(affinityKey, *proxy);
                return response;
            }

            boost::beast::tcp_stream stream(io_);
            stream.expires_after(timeout);
            stream.connect(proxyResults);

            HttpRequest proxiedRequest = request;
            proxiedRequest.target(parsed.scheme + "://" + authorityFrom(parsed) + parsed.target);
            if (auto auth = proxyAuthorization(*proxy); !auth.empty()) {
                proxiedRequest.set("Proxy-Authorization", auth);
            }


            boost::beast::http::write(stream, proxiedRequest);
            boost::beast::flat_buffer buffer;
            HttpResponse response;
            boost::beast::http::read(stream, buffer, response);
            boost::system::error_code ec;
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            if (ec && ec != boost::asio::error::not_connected) {
                throw boost::system::system_error(ec);
            }

            if (response.result() == boost::beast::http::status::proxy_authentication_required) {
                throw std::runtime_error("Proxy authentication required");
            }

            proxyPool_.reportSuccess(affinityKey, *proxy);
            return response;

        }

        boost::asio::ip::tcp::resolver resolver(io_);
        auto results = resolver.resolve(parsed.host, parsed.port);

        if (parsed.scheme == "https") {
            boost::beast::ssl_stream<boost::beast::tcp_stream> stream(io_, sslContext_);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), parsed.host.c_str())) {
                throw std::runtime_error("Failed to set SNI host name");
            }
            auto& lowest = boost::beast::get_lowest_layer(stream);
            lowest.expires_after(timeout);
            lowest.connect(results);
            stream.handshake(boost::asio::ssl::stream_base::client);

            boost::beast::http::write(stream, request);
            boost::beast::flat_buffer buffer;
            HttpResponse response;
            boost::beast::http::read(stream, buffer, response);

            boost::system::error_code ec;
            stream.shutdown(ec);
            if (ec == boost::asio::error::eof || ec == boost::asio::ssl::error::stream_truncated) {
                ec = {};
            }
            if (ec) {
                throw boost::system::system_error(ec);
            }
            return response;
        }


        boost::beast::tcp_stream stream(io_);
        stream.expires_after(timeout);
        stream.connect(results);
        boost::beast::http::write(stream, request);
        boost::beast::flat_buffer buffer;
        HttpResponse response;
        boost::beast::http::read(stream, buffer, response);
        boost::system::error_code ec;
        stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            throw boost::system::system_error(ec);
        }
        return response;
    } catch (...) {
        if (proxy) {
            proxyPool_.reportFailure(affinityKey, *proxy);
        }
        throw;
    }

}

HttpClient::HttpResponse HttpClient::fetch(const std::string& method,
                                           const std::string& url,
                                           const std::vector<Header>& headers,
                                           const std::string& body,
                                           const std::string& affinityKey,
                                           std::chrono::seconds timeout,
                                           bool followRedirects,
                                           unsigned int maxRedirects,
                                           std::string* effectiveUrl,
                                           bool useProxy)
{
    std::string currentUrl = url;
    std::string currentMethod = method;
    std::string currentBody = body;
    HttpResponse response;

    for (unsigned int redirect = 0; redirect <= maxRedirects; ++redirect) {
        ParsedUrl parsed = parseUrl(currentUrl);
        HttpRequest request{toVerb(currentMethod), parsed.target, kHttpVersion};
        request.set(boost::beast::http::field::host, parsed.host);
        request.set("X-Quick-Scheme", parsed.scheme);

        for (const auto& header : headers) {
            request.set(header.name, header.value);
        }

        if (!currentBody.empty() && request.method() != boost::beast::http::verb::get && request.method() != boost::beast::http::verb::head) {
            request.body() = currentBody;
            request.prepare_payload();
        }

        response = fetch(std::move(request), affinityKey, timeout, useProxy);

        if (effectiveUrl) {
            *effectiveUrl = currentUrl;
        }

        if (!followRedirects || !isRedirect(response.result())) {
            return response;
        }

        auto locationIt = response.base().find(boost::beast::http::field::location);
        if (locationIt == response.base().end()) {
            return response;
        }

        std::string location = std::string(locationIt->value());
        currentUrl = combineLocation(parsed, location);

        if (response.result() == boost::beast::http::status::see_other && currentMethod != "GET" && currentMethod != "HEAD") {
            currentMethod = "GET";
            currentBody.clear();
        }
    }

    throw std::runtime_error("Maximum redirect count exceeded");
}

} // namespace quickgrab::util




