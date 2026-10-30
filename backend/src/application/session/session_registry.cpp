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

std::optional<protocol::VerifiedSessionRequest> SessionRegistry::findPendingForRequester(
    const protocol::SessionIdentifier& requestId,
    const protocol::SessionIdentifier& sessionId,
    const std::string& requesterDeviceId,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const auto entry = pending_.find(routeKey(requestId, sessionId));
    if (entry == pending_.end() ||
        entry->second.request.envelope.fromDeviceId != requesterDeviceId) {
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

SessionActivationResult SessionRegistry::activate(
    protocol::VerifiedSessionAccept accept,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expiresAt) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(accept.envelope.requestId, accept.envelope.sessionId);
    const auto pending = pending_.find(key);
    if (pending == pending_.end() || expiresAt <= now ||
        accept.envelope.fromDeviceId != pending->second.request.envelope.toDeviceId ||
        accept.envelope.toDeviceId != pending->second.request.envelope.fromDeviceId)
        return SessionActivationResult::missingPendingRequest;

    ActiveEntry active{
        .session = ActiveSession{
            .request = pending->second.request,
            .accept = std::move(accept),
            .offer = std::nullopt,
            .answer = std::nullopt,
            .requesterGatheringComplete = false,
            .responderGatheringComplete = false,
        },
        .expiresAt = expiresAt,
        .lastRequesterSequence = 1,
        .lastResponderSequence = 1,
        .iceGeneration = std::nullopt,
    };
    const auto [activeEntry, inserted] = active_.emplace(key, std::move(active));
    (void)activeEntry;
    if (!inserted)
        return SessionActivationResult::missingPendingRequest;
    pending_.erase(pending);
    return SessionActivationResult::activated;
}

std::optional<ActiveSession> SessionRegistry::findActiveForParticipant(
    const protocol::SessionIdentifier& requestId,
    const protocol::SessionIdentifier& sessionId,
    const std::string& participantDeviceId,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const auto entry = active_.find(routeKey(requestId, sessionId));
    if (entry == active_.end())
        return std::nullopt;
    const auto& request = entry->second.session.request.envelope;
    if (request.fromDeviceId != participantDeviceId && request.toDeviceId != participantDeviceId)
        return std::nullopt;
    return entry->second.session;
}

NegotiationStoreResult SessionRegistry::storeOffer(
    protocol::VerifiedSessionOffer offer,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(offer.envelope.requestId, offer.envelope.sessionId);
    const auto entry = active_.find(key);
    if (entry == active_.end() ||
        offer.envelope.fromDeviceId != entry->second.session.request.envelope.fromDeviceId ||
        offer.envelope.toDeviceId != entry->second.session.request.envelope.toDeviceId)
        return NegotiationStoreResult::missingSession;
    if (offer.envelope.sequence <= entry->second.lastRequesterSequence)
        return NegotiationStoreResult::replay;
    if (entry->second.iceGeneration &&
        offer.payload.iceGeneration <= *entry->second.iceGeneration) {
        return NegotiationStoreResult::invalidGeneration;
    }

    entry->second.lastRequesterSequence = offer.envelope.sequence;
    entry->second.iceGeneration = offer.payload.iceGeneration;
    entry->second.session.offer = std::move(offer);
    entry->second.session.answer.reset();
    entry->second.session.requesterGatheringComplete = false;
    entry->second.session.responderGatheringComplete = false;
    return NegotiationStoreResult::stored;
}

NegotiationStoreResult SessionRegistry::storeAnswer(
    protocol::VerifiedSessionAnswer answer,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(answer.envelope.requestId, answer.envelope.sessionId);
    const auto entry = active_.find(key);
    if (entry == active_.end() || !entry->second.session.offer ||
        answer.envelope.fromDeviceId != entry->second.session.request.envelope.toDeviceId ||
        answer.envelope.toDeviceId != entry->second.session.request.envelope.fromDeviceId)
        return NegotiationStoreResult::missingSession;
    if (answer.envelope.sequence <= entry->second.lastResponderSequence)
        return NegotiationStoreResult::replay;
    if (!entry->second.iceGeneration ||
        answer.payload.iceGeneration != *entry->second.iceGeneration ||
        entry->second.session.answer) {
        return NegotiationStoreResult::invalidGeneration;
    }

    entry->second.lastResponderSequence = answer.envelope.sequence;
    entry->second.session.answer = std::move(answer);
    return NegotiationStoreResult::stored;
}

NegotiationStoreResult SessionRegistry::storeCandidates(
    protocol::VerifiedSessionCandidates candidates,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(candidates.envelope.requestId, candidates.envelope.sessionId);
    const auto entry = active_.find(key);
    if (entry == active_.end() || !entry->second.session.offer)
        return NegotiationStoreResult::missingSession;
    if (!entry->second.iceGeneration ||
        candidates.payload.iceGeneration != *entry->second.iceGeneration) {
        return NegotiationStoreResult::invalidGeneration;
    }

    auto* lastSequence = &entry->second.lastRequesterSequence;
    auto* gatheringComplete = &entry->second.session.requesterGatheringComplete;
    const auto& request = entry->second.session.request.envelope;
    if (candidates.envelope.fromDeviceId == request.toDeviceId &&
        candidates.envelope.toDeviceId == request.fromDeviceId) {
        if (!entry->second.session.answer)
            return NegotiationStoreResult::invalidGeneration;
        lastSequence = &entry->second.lastResponderSequence;
        gatheringComplete = &entry->second.session.responderGatheringComplete;
    } else if (candidates.envelope.fromDeviceId != request.fromDeviceId ||
               candidates.envelope.toDeviceId != request.toDeviceId) {
        return NegotiationStoreResult::missingSession;
    }

    if (candidates.envelope.sequence <= *lastSequence)
        return NegotiationStoreResult::replay;
    if (*gatheringComplete)
        return NegotiationStoreResult::gatheringComplete;
    *lastSequence = candidates.envelope.sequence;
    return NegotiationStoreResult::stored;
}

NegotiationStoreResult SessionRegistry::storeGatheringComplete(
    protocol::VerifiedSessionGatheringComplete complete,
    std::chrono::steady_clock::time_point now) {
    const std::scoped_lock lock(mutex_);
    removeExpiredLocked(now);
    const std::string key = routeKey(complete.envelope.requestId, complete.envelope.sessionId);
    const auto entry = active_.find(key);
    if (entry == active_.end() || !entry->second.session.offer)
        return NegotiationStoreResult::missingSession;
    if (!entry->second.iceGeneration ||
        complete.payload.iceGeneration != *entry->second.iceGeneration) {
        return NegotiationStoreResult::invalidGeneration;
    }

    auto* lastSequence = &entry->second.lastRequesterSequence;
    auto* gatheringComplete = &entry->second.session.requesterGatheringComplete;
    const auto& request = entry->second.session.request.envelope;
    if (complete.envelope.fromDeviceId == request.toDeviceId &&
        complete.envelope.toDeviceId == request.fromDeviceId) {
        if (!entry->second.session.answer)
            return NegotiationStoreResult::invalidGeneration;
        lastSequence = &entry->second.lastResponderSequence;
        gatheringComplete = &entry->second.session.responderGatheringComplete;
    } else if (complete.envelope.fromDeviceId != request.fromDeviceId ||
               complete.envelope.toDeviceId != request.toDeviceId) {
        return NegotiationStoreResult::missingSession;
    }

    if (complete.envelope.sequence <= *lastSequence)
        return NegotiationStoreResult::replay;
    if (*gatheringComplete)
        return NegotiationStoreResult::gatheringComplete;
    *lastSequence = complete.envelope.sequence;
    *gatheringComplete = true;
    return NegotiationStoreResult::stored;
}

bool SessionRegistry::removeSession(const protocol::SessionIdentifier& requestId,
                                    const protocol::SessionIdentifier& sessionId,
                                    const std::string& participantDeviceId) {
    const std::scoped_lock lock(mutex_);
    const std::string key = routeKey(requestId, sessionId);
    const auto pending = pending_.find(key);
    const auto active = active_.find(key);
    const protocol::SessionEnvelope* request = nullptr;
    if (pending != pending_.end())
        request = &pending->second.request.envelope;
    else if (active != active_.end())
        request = &active->second.session.request.envelope;
    if (request == nullptr ||
        (request->fromDeviceId != participantDeviceId &&
         request->toDeviceId != participantDeviceId)) {
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
    const auto pending = pending_.find(key);
    const auto active = active_.find(key);
    const protocol::SessionEnvelope* envelope = nullptr;
    if (pending != pending_.end())
        envelope = &pending->second.request.envelope;
    else if (active != active_.end())
        envelope = &active->second.session.request.envelope;
    if (envelope == nullptr)
        return;
    const auto source = participantRoutes_.find(envelope->fromDeviceId);
    if (source != participantRoutes_.end() && source->second == key)
        participantRoutes_.erase(source);
    const auto target = participantRoutes_.find(envelope->toDeviceId);
    if (target != participantRoutes_.end() && target->second == key)
        participantRoutes_.erase(target);
    if (pending != pending_.end())
        pending_.erase(pending);
    if (active != active_.end())
        active_.erase(active);
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
    for (auto entry = active_.begin(); entry != active_.end();) {
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
