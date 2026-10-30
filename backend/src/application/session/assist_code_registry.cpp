#include "application/session/assist_code_registry.hpp"

#include "security/crypto/random.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace easydesk::session {
namespace {

constexpr std::uint32_t kCodeSpace = 100'000'000;
constexpr std::uint64_t kAcceptedRange =
    (static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1) /
    kCodeSpace * kCodeSpace;

}  // namespace

AssistCodeLease AssistCodeRegistry::issue(const std::string& deviceId,
                                          std::chrono::steady_clock::time_point now) {
    if (deviceId.empty())
        throw std::invalid_argument("device ID must not be empty");

    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    std::string previousCode;
    const auto previous = byDevice_.find(deviceId);
    if (previous != byDevice_.end()) {
        previousCode = previous->second;
        byCode_.erase(previous->second);
        byDevice_.erase(previous);
    }

    for (int attempt = 0; attempt < 32; ++attempt) {
        std::string code = generateCode();
        if (code == previousCode || byCode_.contains(code))
            continue;
        const auto expiresAt = now + kLifetime;
        byCode_.emplace(code, Entry{.deviceId = deviceId, .expiresAt = expiresAt});
        byDevice_.emplace(deviceId, code);
        return {.code = std::move(code), .expiresAt = expiresAt};
    }
    throw std::runtime_error("assist code space is temporarily unavailable");
}

std::optional<std::string> AssistCodeRegistry::resolve(
    const std::string& code,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const auto entry = byCode_.find(code);
    return entry == byCode_.end() ? std::nullopt
                                 : std::optional<std::string>(entry->second.deviceId);
}

void AssistCodeRegistry::revoke(const std::string& deviceId) {
    const std::scoped_lock lock(mutex_);
    const auto entry = byDevice_.find(deviceId);
    if (entry == byDevice_.end())
        return;
    byCode_.erase(entry->second);
    byDevice_.erase(entry);
}

void AssistCodeRegistry::removeExpiredLocked(std::chrono::steady_clock::time_point now) {
    for (auto entry = byCode_.begin(); entry != byCode_.end();) {
        if (entry->second.expiresAt > now) {
            ++entry;
            continue;
        }
        byDevice_.erase(entry->second.deviceId);
        entry = byCode_.erase(entry);
    }
}

std::string AssistCodeRegistry::generateCode() {
    for (;;) {
        const auto bytes = security::randomBytes(sizeof(std::uint32_t));
        const std::uint32_t value = static_cast<std::uint32_t>(bytes[0]) |
                                    (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                                    (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                                    (static_cast<std::uint32_t>(bytes[3]) << 24U);
        if (value >= kAcceptedRange)
            continue;
        std::array<char, 9> output{};
        std::snprintf(output.data(), output.size(), "%08u", value % kCodeSpace);
        return output.data();
    }
}

}  // namespace easydesk::session
