#include "application/session/session_registry.hpp"

#include <utility>

namespace easydesk::session {

PendingRequestResult SessionRegistry::addPending(protocol::VerifiedSessionRequest request,
                                                 std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(request.envelope.requestId, request.envelope.sessionId);
    if (pending_.contains(key))
        return PendingRequestResult::duplicate;
    if (participantRoutes_.contains(request.envelope.fromDeviceId) ||
        participantRoutes_.contains(request.envelope.toDeviceId)) {
        return PendingRequestResult::participantBusy;
    }

    participantRoutes_.emplace(request.envelope.fromDeviceId, key);
    participantRoutes_.emplace(request.envelope.toDeviceId, key);
    pending_.emplace(key, PendingEntry{
                              .request = std::move(request),
                              .expiresAt = now + kConsentTimeout,
                          });
    return PendingRequestResult::stored;
}

std::optional<protocol::VerifiedSessionRequest> SessionRegistry::findPendingForResponder(
    const protocol::SessionIdentifier& requestId,
    const protocol::SessionIdentifier& sessionId,
    const std::string& responderDeviceId,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const auto entry = pending_.find(routeKey(requestId, sessionId));
    if (entry == pending_.end() ||
        entry->second.request.envelope.toDeviceId != responderDeviceId) {
        return std::nullopt;
    }
    return entry->second.request;
}

bool SessionRegistry::consumePending(const protocol::SessionIdentifier& requestId,
                                     const protocol::SessionIdentifier& sessionId,
                                     const std::string& participantDeviceId,
                                     std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(requestId, sessionId);
    const auto entry = pending_.find(key);
    if (entry == pending_.end() ||
        (entry->second.request.envelope.fromDeviceId != participantDeviceId &&
         entry->second.request.envelope.toDeviceId != participantDeviceId)) {
        return false;
    }
    removeLocked(key);
    return true;
}

void SessionRegistry::removeForDevice(const std::string& deviceId) {
    const std::scoped_lock lock(mutex_);
    const auto route = participantRoutes_.find(deviceId);
    if (route != participantRoutes_.end())
        removeLocked(route->second);
}

std::string SessionRegistry::routeKey(const protocol::SessionIdentifier& requestId,
                                      const protocol::SessionIdentifier& sessionId) {
    std::string key;
    key.reserve(requestId.size() + sessionId.size());
    key.append(reinterpret_cast<const char*>(requestId.data()), requestId.size());
    key.append(reinterpret_cast<const char*>(sessionId.data()), sessionId.size());
    return key;
}

void SessionRegistry::removeLocked(const std::string& key) {
    const auto entry = pending_.find(key);
    if (entry == pending_.end())
        return;
    const auto& envelope = entry->second.request.envelope;
    const auto source = participantRoutes_.find(envelope.fromDeviceId);
    if (source != participantRoutes_.end() && source->second == key)
        participantRoutes_.erase(source);
    const auto target = participantRoutes_.find(envelope.toDeviceId);
    if (target != participantRoutes_.end() && target->second == key)
        participantRoutes_.erase(target);
    pending_.erase(entry);
}

void SessionRegistry::removeExpiredLocked(std::chrono::steady_clock::time_point now) {
    for (auto entry = pending_.begin(); entry != pending_.end();) {
        if (entry->second.expiresAt > now) {
            ++entry;
            continue;
        }
        const std::string key = entry->first;
        ++entry;
        removeLocked(key);
    }
}

}  // namespace easydesk::session
