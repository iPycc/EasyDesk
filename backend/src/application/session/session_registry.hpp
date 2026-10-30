#pragma once

#include "protocol/session/session_verifier.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace easydesk::session {

enum class PendingRequestResult {
    stored,
    duplicate,
    participantBusy,
};

class SessionRegistry final {
public:
    static constexpr auto kConsentTimeout = std::chrono::seconds(60);

    PendingRequestResult addPending(protocol::VerifiedSessionRequest request,
                                    std::chrono::steady_clock::time_point now);
    std::optional<protocol::VerifiedSessionRequest> findPendingForResponder(
        const protocol::SessionIdentifier& requestId,
        const protocol::SessionIdentifier& sessionId,
        const std::string& responderDeviceId,
        std::chrono::steady_clock::time_point now);
    bool consumePending(const protocol::SessionIdentifier& requestId,
                        const protocol::SessionIdentifier& sessionId,
                        const std::string& participantDeviceId,
                        std::chrono::steady_clock::time_point now);
    void removeForDevice(const std::string& deviceId);

private:
    struct PendingEntry {
        protocol::VerifiedSessionRequest request;
        std::chrono::steady_clock::time_point expiresAt;
    };

    static std::string routeKey(const protocol::SessionIdentifier& requestId,
                                const protocol::SessionIdentifier& sessionId);
    void removeLocked(const std::string& key);
    void removeExpiredLocked(std::chrono::steady_clock::time_point now);

    std::mutex mutex_;
    std::unordered_map<std::string, PendingEntry> pending_;
    std::unordered_map<std::string, std::string> participantRoutes_;
};

}  // namespace easydesk::session
