#include "quickgrab/util/WeidianParser.hpp"
#include "quickgrab/util/Logging.hpp"

#include <boost/json.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

namespace quickgrab::util {
namespace {

std::string decodeHtmlAttribute(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '&') {
            auto semi = input.find(';', i + 1);
            if (semi == std::string::npos) {
                output.push_back('&');
                continue;
            }
            auto entity = input.substr(i + 1, semi - i - 1);
            if (entity == "quot") {
                output.push_back('"');
            } else if (entity == "amp") {
                output.push_back('&');
            } else if (entity == "lt") {
                output.push_back('<');
            } else if (entity == "gt") {
                output.push_back('>');
            } else if (!entity.empty() && entity[0] == '#') {
                char ch = '?';
                if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                    ch = static_cast<char>(std::strtol(entity.c_str() + 2, nullptr, 16));
                } else {
                    ch = static_cast<char>(std::strtol(entity.c_str() + 1, nullptr, 10));
                }
                output.push_back(ch);
            } else {
                output.append("&").append(entity).append(";");
            }
            i = semi;
        } else {
            output.push_back(input[i]);
        }
    }
    return output;
}

} // namespace

std::optional<boost::json::value> extractDataObject(const std::string& html) {
    auto markerPos = html.find("__rocker-render-inject__");
    if (markerPos == std::string::npos) {
        return std::nullopt;
    }
    auto attrPos = html.find("data-obj", markerPos);
    if (attrPos == std::string::npos) {
        return std::nullopt;
    }
    attrPos = html.find('=', attrPos);
    if (attrPos == std::string::npos) {
        return std::nullopt;
    }
    ++attrPos;
    while (attrPos < html.size() && std::isspace(static_cast<unsigned char>(html[attrPos]))) {
        ++attrPos;
    }
    if (attrPos >= html.size()) {
        return std::nullopt;
    }
    char quote = html[attrPos];
    if (quote != '"' && quote != '\'') {
        return std::nullopt;
    }
    ++attrPos;
    auto end = html.find(quote, attrPos);
    if (end == std::string::npos) {
        return std::nullopt;
    }
    std::string raw = html.substr(attrPos, end - attrPos);
    std::string decoded = decodeHtmlAttribute(raw);
    try {
        return boost::json::parse(decoded);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::warn, std::string{"Failed to parse data-obj JSON: "} + ex.what());
        return std::nullopt;
    }
}

} // namespace quickgrab::util
