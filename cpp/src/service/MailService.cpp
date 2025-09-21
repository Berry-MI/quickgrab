#include "quickgrab/service/MailService.hpp"
#include "quickgrab/util/JsonUtil.hpp"
#include "quickgrab/util/Logging.hpp"

#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace quickgrab::service {
namespace {
boost::json::object toObject(const boost::json::value& value) {
    if (value.is_object()) {
        return value.as_object();
    }
    return {};
}

std::string joinStrings(const std::vector<std::string>& parts, std::string_view separator) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << separator;
        }
        oss << parts[i];
    }
    return oss.str();
}

std::string timestampString() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

std::string extractOrderLink(const boost::json::object& response) {
    if (auto* list = response.if_contains("orderLink_list"); list && list->is_array() && !list->as_array().empty()) {
        const auto& first = list->as_array().front();
        if (first.is_object()) {
            const auto& obj = first.as_object();
            if (auto* link = obj.if_contains("orderLink"); link && link->is_string()) {
                return std::string(link->as_string());
            }
        }
    }
    return {};
}

std::string extractOrderDesc(const boost::json::object& response) {
    if (auto* list = response.if_contains("orderLink_list"); list && list->is_array() && !list->as_array().empty()) {
        const auto& first = list->as_array().front();
        if (first.is_object()) {
            const auto& obj = first.as_object();
            if (auto* desc = obj.if_contains("desc"); desc && desc->is_string()) {
                return std::string(desc->as_string());
            }
        }
    }
    return {};
}

std::string extractErrorMessage(const boost::json::object& response) {
    if (auto* status = response.if_contains("status"); status && status->is_object()) {
        const auto& obj = status->as_object();
        if (auto* description = obj.if_contains("description"); description && description->is_string()) {
            return std::string(description->as_string());
        }
    }
    if (auto* message = response.if_contains("error_message"); message && message->is_string()) {
        return std::string(message->as_string());
    }
    return {};
}

} // namespace

MailService::MailService(Config config)
    : config_(std::move(config)) {}

bool MailService::sendSuccessEmail(const model::Request& request,
                                   const workflow::GrabResult& result) {
    auto extension = toObject(request.extension);
    if (!shouldNotify(extension)) {
        return false;
    }

    auto email = getString(extension, "email");
    if (!email || email->find('@') == std::string::npos) {
        return false;
    }

    auto response = result.response.is_object() ? result.response.as_object() : boost::json::object{};
    std::string link = extractOrderLink(response);
    if (link.empty()) {
        return false;
    }

    auto userInfo = toObject(request.userInfo);
    auto phone = getString(userInfo, "telephone").value_or("");
    auto nick = getString(userInfo, "nickName").value_or("");
    std::string phoneDisplay = phone;
    if (!nick.empty()) {
        phoneDisplay += "(" + nick + ")";
    }

    std::string infoBox = renderInfoBox("手机号", phoneDisplay, "#2ecc71");
    std::string description = extractOrderDesc(response);
    std::string descBox = description.empty() ? std::string{} : renderDescriptionBox(description);

    auto body = renderEmail("#2ecc71",
                            "✅ 下单成功",
                            "请及时完成支付",
                            infoBox,
                            link,
                            "linear-gradient(135deg, #2ecc71, #27ae60)",
                            "rgba(46,204,113,0.3)",
                            "立即支付",
                            descBox,
                            "如非本人操作，请忽略本通知");

    return deliver(*email, "微店下单成功通知", body);
}

bool MailService::sendFailureEmail(const model::Request& request,
                                   const workflow::GrabResult& result) {
    auto extension = toObject(request.extension);
    if (!shouldNotify(extension)) {
        return false;
    }

    auto email = getString(extension, "email");
    if (!email || email->find('@') == std::string::npos) {
        return false;
    }

    auto response = result.response.is_object() ? result.response.as_object() : boost::json::object{};
    std::string reason = extractErrorMessage(response);
    if (reason.empty()) {
        reason = !result.message.empty() ? result.message : result.error;
    }
    if (reason.empty()) {
        reason = "抢购失败，请检查商品状态";
    }

    auto userInfo = toObject(request.userInfo);
    auto phone = getString(userInfo, "telephone").value_or("");
    auto nick = getString(userInfo, "nickName").value_or("");
    std::string phoneDisplay = phone;
    if (!nick.empty()) {
        phoneDisplay += "(" + nick + ")";
    }

    std::string infoBox = renderInfoBox("手机号", phoneDisplay, "#3498db");
    std::string errorBox = renderErrorBox(reason);

    auto body = renderEmail("#e74c3c",
                            "❌ 抢购失败",
                            "请查看失败原因",
                            errorBox + infoBox,
                            request.link,
                            "linear-gradient(135deg, #3498db, #2980b9)",
                            "rgba(52,152,219,0.3)",
                            "查看详情",
                            std::string{},
                            "如需帮助，请联系客服");

    return deliver(*email, "微店抢购失败通知", body);
}

bool MailService::sendFoundItemEmail(const model::Request& request,
                                     const std::string& link) {
    auto extension = toObject(request.extension);
    if (!shouldNotify(extension)) {
        return false;
    }

    auto email = getString(extension, "email");
    if (!email || email->find('@') == std::string::npos) {
        return false;
    }

    auto userInfo = toObject(request.userInfo);
    auto phone = getString(userInfo, "telephone").value_or("");
    auto nick = getString(userInfo, "nickName").value_or("");
    std::string phoneDisplay = phone;
    if (!nick.empty()) {
        phoneDisplay += "(" + nick + ")";
    }

    std::vector<std::string> titles;
    std::string keyword = request.keyword;
    auto pos = keyword.find('|');
    if (pos != std::string::npos) {
        titles.emplace_back(keyword.substr(0, pos));
        titles.emplace_back(keyword.substr(pos + 1));
    } else {
        titles.emplace_back(keyword);
    }

    std::string infoBox = renderInfoBox("手机号", phoneDisplay, "#1890ff");
    std::string desc = joinStrings(titles, "\n");
    std::string descBox = renderDescriptionBox("商品信息:\n" + desc);

    auto body = renderEmail("#1890ff",
                            "🔍 找到商品",
                            "已找到符合条件的商品",
                            infoBox,
                            link,
                            "linear-gradient(135deg, #1890ff, #096dd9)",
                            "rgba(24,144,255,0.3)",
                            "查看商品",
                            descBox,
                            "如非本人操作，请忽略本通知");

    return deliver(*email, "找到符合条件的商品", body);
}

std::optional<std::string> MailService::getString(const boost::json::object& obj, std::string_view key) {
    if (auto* value = obj.if_contains(key); value && value->is_string()) {
        return std::string(value->as_string());
    }
    return std::nullopt;
}

bool MailService::isTruthy(const boost::json::object& obj, std::string_view key) {
    if (auto* value = obj.if_contains(key)) {
        if (value->is_bool()) {
            return value->as_bool();
        }
        if (value->is_int64()) {
            return value->as_int64() != 0;
        }
    }
    return false;
}

std::string MailService::sanitizeFilename(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
            result.push_back(c);
        } else {
            result.push_back('_');
        }
    }
    return result;
}

bool MailService::shouldNotify(const boost::json::object& extension) const {
    return isTruthy(extension, "emailReminder");
}

std::string MailService::renderInfoBox(std::string_view label,
                                       std::string_view value,
                                       std::string_view color) const {
    std::ostringstream oss;
    oss << "<div style='background-color: #f8f9fa; border-left: 4px solid " << color
        << "; padding: 15px; margin-bottom: 20px;'>\n"
        << "    <p style='margin: 0; font-size: 15px; color: #2c3e50;'>\n"
        << "        <strong>" << label << "：</strong>\n"
        << "        <span style='color: #34495e;'>" << value << "</span>\n"
        << "    </p>\n"
        << "</div>\n";
    return oss.str();
}

std::string MailService::renderErrorBox(std::string_view reason) const {
    std::ostringstream oss;
    oss << "<div style='background-color: #fff5f5; border-left: 4px solid #e74c3c; padding: 15px; margin-bottom: 20px;'>\n"
        << "    <p style='margin: 0; font-size: 15px; color: #c0392b;'>\n"
        << "        <strong>异常原因：</strong>\n"
        << "        <span>" << reason << "</span>\n"
        << "    </p>\n"
        << "</div>\n";
    return oss.str();
}

std::string MailService::renderDescriptionBox(std::string_view desc) const {
    std::ostringstream oss;
    oss << "<div style='background-color: #fff8f0; border-radius: 8px; padding: 15px; margin-top: 20px;'>\n"
        << "    <p style='margin: 0; color: #e67e22; font-size: 14px;'>\n"
        << "        <span style='font-weight: bold;'>📝 订单说明：</span>\n"
        << "        " << desc << "\n"
        << "    </p>\n"
        << "</div>\n";
    return oss.str();
}

std::string MailService::renderEmail(std::string_view accentColor,
                                     std::string_view title,
                                     std::string_view subtitle,
                                     std::string_view infoBox,
                                     std::string_view link,
                                     std::string_view buttonGradient,
                                     std::string_view buttonShadow,
                                     std::string_view buttonLabel,
                                     std::string_view description,
                                     std::string_view footer) const {
    std::ostringstream oss;
    oss << "<div style='max-width: 600px; margin: 0 auto; font-family: Arial, sans-serif; line-height: 1.6; background-color: #f9f9f9; padding: 20px;'>\n"
        << "    <div style='background-color: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);'>\n"
        << "        <div style='text-align: center; margin-bottom: 30px;'>\n"
        << "            <h1 style='color: " << accentColor << "; font-size: 24px; margin: 0;'>" << title << "</h1>\n"
        << "            <p style='color: #7f8c8d; font-size: 14px; margin-top: 5px;'>" << subtitle << "</p>\n"
        << "        </div>\n"
        << infoBox
        << "        <div style='margin: 25px 0; text-align: center;'>\n"
        << "            <a href='" << link << "' style='display: inline-block; padding: 12px 30px; font-size: 16px; color: #fff; background: "
        << buttonGradient
        << "; text-decoration: none; border-radius: 50px; box-shadow: 0 3px 6px " << buttonShadow
        << "; transition: transform 0.2s;'>\n"
        << "                " << buttonLabel << " →\n"
        << "            </a>\n"
        << "        </div>\n"
        << description
        << "        <div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center; color: #95a5a6; font-size: 12px;'>\n"
        << "            <p>" << footer << "</p>\n"
        << "            <p style='margin: 5px 0;'>本邮件由系统自动发送，请勿回复</p>\n"
        << "        </div>\n"
        << "    </div>\n"
        << "</div>\n";
    return oss.str();
}

bool MailService::deliver(const std::string& to,
                          const std::string& subject,
                          const std::string& body) const {
    try {
        std::filesystem::create_directories(config_.spoolDirectory);
        auto filename = timestampString() + "-" + sanitizeFilename(to) + ".html";
        auto path = config_.spoolDirectory / filename;
        std::ofstream ofs(path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            throw std::runtime_error("无法写入邮件文件: " + path.string());
        }
        ofs << "Subject: " << subject << "\n";
        ofs << "To: " << to << "\n";
        ofs << "From: " << config_.senderName << " <" << config_.fromEmail << ">\n\n";
        ofs << body;
        util::log(util::LogLevel::info,
                  "邮件已写入 " + path.string() + " subject=" + subject + " to=" + to);
        return true;
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"写入邮件失败: "} + ex.what());
        return false;
    }
}

} // namespace quickgrab::service
