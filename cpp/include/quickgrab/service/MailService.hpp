#pragma once

#include "quickgrab/model/Request.hpp"
#include "quickgrab/workflow/GrabWorkflow.hpp"

#include <boost/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace quickgrab::service {

class MailService {
public:
    struct Config {
        std::string fromEmail = "1966099953@qq.com";
        std::string senderName = "微店任务通知";
        std::filesystem::path spoolDirectory = std::filesystem::path{"data"} / "outbox";
    };

    explicit MailService(Config config = Config{});

    bool sendSuccessEmail(const model::Request& request,
                          const workflow::GrabResult& result);
    bool sendFailureEmail(const model::Request& request,
                          const workflow::GrabResult& result);
    bool sendFoundItemEmail(const model::Request& request,
                            const std::string& link);

private:
    static std::optional<std::string> getString(const boost::json::object& obj, std::string_view key);
    static bool isTruthy(const boost::json::object& obj, std::string_view key);
    static std::string sanitizeFilename(std::string_view input);

    bool shouldNotify(const boost::json::object& extension) const;
    std::string renderInfoBox(std::string_view label,
                              std::string_view value,
                              std::string_view color) const;
    std::string renderErrorBox(std::string_view reason) const;
    std::string renderDescriptionBox(std::string_view desc) const;
    std::string renderEmail(std::string_view accentColor,
                            std::string_view title,
                            std::string_view subtitle,
                            std::string_view infoBox,
                            std::string_view link,
                            std::string_view buttonGradient,
                            std::string_view buttonShadow,
                            std::string_view buttonLabel,
                            std::string_view description,
                            std::string_view footer) const;
    bool deliver(const std::string& to,
                 const std::string& subject,
                 const std::string& body) const;

    Config config_;
};

} // namespace quickgrab::service
