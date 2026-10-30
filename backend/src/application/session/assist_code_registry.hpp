#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace easydesk::session {

struct AssistCodeLease {
    std::string code;
    std::chrono::steady_clock::time_point expiresAt;
};

class AssistCodeRegistry final {
public:
    static constexpr auto kLifetime = std::chrono::minutes(10);

    AssistCodeLease issue(const std::string& deviceId,
                          std::chrono::steady_clock::time_point now);
    std::optional<std::string> resolve(const std::string& code,
                                       std::chrono::steady_clock::time_point now);
    void revoke(const std::string& deviceId);

private:
    struct Entry {
        std::string deviceId;
        std::chrono::steady_clock::time_point expiresAt;
    };

    void removeExpiredLocked(std::chrono::steady_clock::time_point now);
    static std::string generateCode();

    std::mutex mutex_;
    std::unordered_map<std::string, Entry> byCode_;
    std::unordered_map<std::string, std::string> byDevice_;
};

}  // namespace easydesk::session
