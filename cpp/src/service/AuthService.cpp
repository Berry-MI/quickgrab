#include "quickgrab/service/AuthService.hpp"
#include "quickgrab/util/Logging.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>

namespace quickgrab::service {
namespace {
std::string trim(const std::string& input) {
    auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

model::Buyer sanitizeBuyer(model::Buyer buyer) {
    buyer.password.clear();
    return buyer;
}
} // namespace

AuthService::AuthService(repository::BuyersRepository& buyers,
                         std::chrono::seconds defaultTtl,
                         std::chrono::seconds rememberTtl)
    : buyers_(buyers)
    , defaultTtl_(defaultTtl)
    , rememberTtl_(rememberTtl) {}

AuthService::AuthResult AuthService::authenticate(const std::string& username,
                                                  const std::string& password,
                                                  bool rememberMe) {
    AuthResult result;

    const std::string trimmedUser = trim(username);
    const std::string trimmedPassword = trim(password);
    if (trimmedUser.empty() || trimmedPassword.empty()) {
        result.message = "账号或密码错误";
        return result;
    }

    std::optional<model::Buyer> buyer;
    try {
        buyer = buyers_.findByUsername(trimmedUser);
    } catch (const std::exception& ex) {
        util::log(util::LogLevel::error, std::string{"验证用户失败: "} + ex.what());
        result.message = "服务器内部错误";
        result.serverError = true;
        return result;
    }

    if (!buyer) {
        result.message = "账号或密码错误";
        return result;
    }

    if (buyer->password != trimmedPassword) {
        result.message = "账号或密码错误";
        return result;
    }

    if (buyer->accessLevel <= 0) {
        result.message = "账号或密码错误";
        return result;
    }

    if (buyer->validityPeriod && std::chrono::system_clock::now() > *buyer->validityPeriod) {
        result.message = "账号已过期";
        return result;
    }

    const auto now = std::chrono::system_clock::now();
    const auto ttl = rememberMe ? rememberTtl_ : defaultTtl_;
    const auto expiresAt = now + ttl;

    SessionInfo sessionInfo;
    sessionInfo.rememberMe = rememberMe;
    sessionInfo.expiresAt = expiresAt;
    sessionInfo.buyer = sanitizeBuyer(*buyer);

    std::unique_lock<std::mutex> lock(mutex_);
    purgeExpiredLocked(now);

    do {
        sessionInfo.token = generateToken();
    } while (sessions_.find(sessionInfo.token) != sessions_.end());

    sessions_.emplace(sessionInfo.token,
                      StoredSession{sanitizeBuyer(*buyer), expiresAt, rememberMe});

    result.success = true;
    result.message = "Login successful";
    result.session = sessionInfo;
    return result;
}

void AuthService::logout(const std::string& token) {
    if (token.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

std::optional<model::Buyer> AuthService::getBuyerByToken(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    purgeExpiredLocked(now);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return std::nullopt;
    }

    if (it->second.expiresAt <= now) {
        sessions_.erase(it);
        return std::nullopt;
    }

    return it->second.buyer;
}

std::string AuthService::generateToken() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<unsigned long long> dist(0, std::numeric_limits<unsigned long long>::max());
    unsigned long long value = dist(rng);
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 4; ++i) {
        oss << std::setw(16) << std::setfill('0') << value;
        value = dist(rng);
    }
    return oss.str();
}

void AuthService::purgeExpiredLocked(std::chrono::system_clock::time_point now) {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expiresAt <= now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace quickgrab::service

