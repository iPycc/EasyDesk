#include "application/session/session_attempt_limiter.hpp"

#include <iterator>

namespace easydesk::session {

AttemptLimitResult SessionAttemptLimiter::beginOutgoing(
    const std::string& deviceId,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeIdleLocked(now);
    State& state = states_[deviceId];
    state.lastTouched = now;
    prune(state.outgoing, now);
    if (state.cooldownUntil > now)
        return AttemptLimitResult::cooldown;
    if (state.outgoing.size() >= kAttemptsPerMinute)
        return AttemptLimitResult::rateLimited;
    state.outgoing.push_back(now);
    return AttemptLimitResult::allowed;
}

AttemptLimitResult SessionAttemptLimiter::beginIncoming(
    const std::string& deviceId,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeIdleLocked(now);
    State& state = states_[deviceId];
    state.lastTouched = now;
    prune(state.incoming, now);
    if (state.incoming.size() >= kAttemptsPerMinute)
        return AttemptLimitResult::rateLimited;
    state.incoming.push_back(now);
    return AttemptLimitResult::allowed;
}

void SessionAttemptLimiter::recordFailure(const std::string& deviceId,
                                          std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    State& state = states_[deviceId];
    state.lastTouched = now;
    ++state.consecutiveFailures;
    if (state.consecutiveFailures >= kFailuresBeforeCooldown) {
        state.consecutiveFailures = 0;
        state.cooldownUntil = now + kCooldown;
    }
}

void SessionAttemptLimiter::recordSuccess(const std::string& deviceId,
                                          std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    State& state = states_[deviceId];
    state.lastTouched = now;
    state.consecutiveFailures = 0;
    state.cooldownUntil = {};
}

void SessionAttemptLimiter::prune(
    std::deque<std::chrono::steady_clock::time_point>& attempts,
    std::chrono::steady_clock::time_point now) {
    const auto cutoff = now - kWindow;
    while (!attempts.empty() && attempts.front() <= cutoff)
        attempts.pop_front();
}

void SessionAttemptLimiter::removeIdleLocked(std::chrono::steady_clock::time_point now) {
    if (states_.size() < 512)
        return;
    for (auto entry = states_.begin(); entry != states_.end();) {
        prune(entry->second.outgoing, now);
        prune(entry->second.incoming, now);
        const bool idle = entry->second.outgoing.empty() && entry->second.incoming.empty() &&
                          entry->second.consecutiveFailures == 0 &&
                          entry->second.cooldownUntil <= now &&
                          entry->second.lastTouched + kWindow <= now;
        entry = idle ? states_.erase(entry) : std::next(entry);
    }
}

}  // namespace easydesk::session
