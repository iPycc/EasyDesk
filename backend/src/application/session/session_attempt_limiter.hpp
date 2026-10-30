#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace easydesk::session {

enum class AttemptLimitResult {
    allowed,
    rateLimited,
    cooldown,
};

class SessionAttemptLimiter final {
public:
    static constexpr std::size_t kAttemptsPerMinute = 5;
    static constexpr std::size_t kFailuresBeforeCooldown = 5;
    static constexpr auto kWindow = std::chrono::minutes(1);
    static constexpr auto kCooldown = std::chrono::seconds(30);

    AttemptLimitResult beginOutgoing(const std::string& deviceId,
                                     std::chrono::steady_clock::time_point now);
    AttemptLimitResult beginIncoming(const std::string& deviceId,
                                     std::chrono::steady_clock::time_point now);
    void recordFailure(const std::string& deviceId,
                       std::chrono::steady_clock::time_point now);
    void recordSuccess(const std::string& deviceId,
                       std::chrono::steady_clock::time_point now);

private:
    struct State {
        std::deque<std::chrono::steady_clock::time_point> outgoing;
        std::deque<std::chrono::steady_clock::time_point> incoming;
        std::size_t consecutiveFailures = 0;
        std::chrono::steady_clock::time_point cooldownUntil{};
        std::chrono::steady_clock::time_point lastTouched{};
    };

    static void prune(std::deque<std::chrono::steady_clock::time_point>& attempts,
                      std::chrono::steady_clock::time_point now);
    void removeIdleLocked(std::chrono::steady_clock::time_point now);

    std::mutex mutex_;
    std::unordered_map<std::string, State> states_;
};

}  // namespace easydesk::session
