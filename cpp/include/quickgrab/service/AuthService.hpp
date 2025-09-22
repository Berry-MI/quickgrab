#pragma once

#include "quickgrab/model/Buyer.hpp"
#include "quickgrab/repository/BuyersRepository.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace quickgrab::service {

class AuthService {
public:
    struct SessionInfo {
        std::string token;
        model::Buyer buyer;
        std::chrono::system_clock::time_point expiresAt;
        bool rememberMe{false};
    };

    static inline constexpr std::string_view kSessionCookie = "quickgrab_session";

    AuthService(repository::BuyersRepository& buyers,
                std::chrono::seconds defaultTtl = std::chrono::hours(12),
                std::chrono::seconds rememberTtl = std::chrono::seconds{1296000});

    struct AuthResult {
        bool success{false};
        bool serverError{false};
        std::string message;
        std::optional<SessionInfo> session;
    };

    AuthResult authenticate(const std::string& username,
                            const std::string& password,
                            bool rememberMe);

    void logout(const std::string& token);

    std::optional<model::Buyer> getBuyerByToken(const std::string& token);

private:
    struct StoredSession {
        model::Buyer buyer;
        std::chrono::system_clock::time_point expiresAt;
        bool rememberMe;
    };

    repository::BuyersRepository& buyers_;
    std::chrono::seconds defaultTtl_;
    std::chrono::seconds rememberTtl_;
    std::mutex mutex_;
    std::unordered_map<std::string, StoredSession> sessions_;

    std::string generateToken();
    void purgeExpiredLocked(std::chrono::system_clock::time_point now);
};

} // namespace quickgrab::service

