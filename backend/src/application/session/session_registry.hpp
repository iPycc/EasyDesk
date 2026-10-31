#pragma once

#include "protocol/session/session_verifier.hpp"

#include <chrono>
#include <cstdint>
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

enum class SessionActivationResult {
    activated,
    missingPendingRequest,
};

enum class SessionTerminationResult {
    ended,
    missingSession,
    replay,
    staleAuthorization,
};

enum class PermissionStoreResult {
    stored,
    missingSession,
    replay,
    staleAuthorization,
};

enum class NegotiationStoreResult {
    stored,
    missingSession,
    replay,
    invalidGeneration,
    gatheringComplete,
};

struct ActiveSession {
    protocol::VerifiedSessionRequest request;
    protocol::VerifiedSessionAccept accept;
    std::optional<protocol::VerifiedSessionOffer> offer;
    std::optional<protocol::VerifiedSessionAnswer> answer;
    std::optional<protocol::VerifiedSessionRevoke> revoke;
    bool requesterGatheringComplete = false;
    bool responderGatheringComplete = false;
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
    std::optional<protocol::VerifiedSessionRequest> findPendingForRequester(
        const protocol::SessionIdentifier& requestId,
        const protocol::SessionIdentifier& sessionId,
        const std::string& requesterDeviceId,
        std::chrono::steady_clock::time_point now);
    bool consumePending(const protocol::SessionIdentifier& requestId,
                        const protocol::SessionIdentifier& sessionId,
                        const std::string& participantDeviceId,
                        std::chrono::steady_clock::time_point now);
    SessionActivationResult activate(protocol::VerifiedSessionAccept accept,
                                     std::chrono::steady_clock::time_point now,
                                     std::chrono::steady_clock::time_point expiresAt);
    std::optional<ActiveSession> findActiveForParticipant(
        const protocol::SessionIdentifier& requestId,
        const protocol::SessionIdentifier& sessionId,
        const std::string& participantDeviceId,
        std::chrono::steady_clock::time_point now);
    NegotiationStoreResult storeOffer(protocol::VerifiedSessionOffer offer,
                                      std::chrono::steady_clock::time_point now);
    NegotiationStoreResult storeAnswer(protocol::VerifiedSessionAnswer answer,
                                       std::chrono::steady_clock::time_point now);
    NegotiationStoreResult storeCandidates(protocol::VerifiedSessionCandidates candidates,
                                           std::chrono::steady_clock::time_point now);
    NegotiationStoreResult storeGatheringComplete(
        protocol::VerifiedSessionGatheringComplete complete,
        std::chrono::steady_clock::time_point now);
    PermissionStoreResult storeRevoke(protocol::VerifiedSessionRevoke revoke,
                                      std::chrono::steady_clock::time_point now);
    SessionTerminationResult endSession(const protocol::VerifiedSessionEnd& end,
                                        std::chrono::steady_clock::time_point now);
    bool removeSession(const protocol::SessionIdentifier& requestId,
                       const protocol::SessionIdentifier& sessionId,
                       const std::string& participantDeviceId);
    void removeForDevice(const std::string& deviceId);

private:
    struct PendingEntry {
        protocol::VerifiedSessionRequest request;
        std::chrono::steady_clock::time_point expiresAt;
    };

    struct ActiveEntry {
        ActiveSession session;
        std::chrono::steady_clock::time_point expiresAt;
        std::uint64_t lastRequesterSequence = 1;
        std::uint64_t lastResponderSequence = 1;
        std::optional<std::uint32_t> iceGeneration;
    };

    static std::string routeKey(const protocol::SessionIdentifier& requestId,
                                const protocol::SessionIdentifier& sessionId);
    void removeLocked(const std::string& key);
    void removeExpiredLocked(std::chrono::steady_clock::time_point now);

    std::mutex mutex_;
    std::unordered_map<std::string, PendingEntry> pending_;
    std::unordered_map<std::string, ActiveEntry> active_;
    std::unordered_map<std::string, std::string> participantRoutes_;
};

}  // namespace easydesk::session
