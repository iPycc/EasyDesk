#include "protocol/session/session_verifier.hpp"

#include "security/crypto/ed25519.hpp"

#include <optional>
#include <utility>

namespace easydesk::protocol {
namespace {

struct SessionSignalBinding {
    const SessionPublicKey* signerPublicKey;
    SessionDigest descriptionDigest;
    std::uint32_t iceGeneration;
    std::uint64_t descriptionSequence;
};

std::optional<SessionSignalBinding> sessionSignalBinding(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer,
    const VerifiedSessionAnswer* answer) {
    if (!envelope.sessionEpoch || !envelope.iceGeneration ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId ||
        *envelope.sessionEpoch != accept.payload.sessionEpoch) {
        return std::nullopt;
    }

    if (envelope.fromDeviceId == request.envelope.fromDeviceId &&
        envelope.toDeviceId == request.envelope.toDeviceId) {
        return SessionSignalBinding{
            .signerPublicKey = &request.payload.requesterPublicKey,
            .descriptionDigest = sessionPayloadDigest(offer.envelope.signedPayload),
            .iceGeneration = offer.payload.iceGeneration,
            .descriptionSequence = offer.envelope.sequence,
        };
    }
    if (answer && envelope.fromDeviceId == request.envelope.toDeviceId &&
        envelope.toDeviceId == request.envelope.fromDeviceId) {
        return SessionSignalBinding{
            .signerPublicKey = &request.payload.targetPublicKey,
            .descriptionDigest = sessionPayloadDigest(answer->envelope.signedPayload),
            .iceGeneration = answer->payload.iceGeneration,
            .descriptionSequence = answer->envelope.sequence,
        };
    }
    return std::nullopt;
}

}  // namespace

SessionRequestVerificationResult verifySessionRequest(
    const SessionEnvelope& envelope,
    const SessionPublicKey& registeredSourcePublicKey,
    const SessionPublicKey& registeredTargetPublicKey) {
    if (envelope.type != SessionMessageType::request)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence != 1 || envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.fromDeviceId == envelope.toDeviceId ||
        registeredSourcePublicKey == registeredTargetPublicKey) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(registeredSourcePublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionRequestPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->requestId != envelope.requestId || payload->sessionId != envelope.sessionId ||
        payload->requesterPublicKey != registeredSourcePublicKey ||
        payload->targetPublicKey != registeredTargetPublicKey ||
        payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionRequest{.envelope = envelope, .payload = std::move(*payload)};
}

SessionAcceptVerificationResult verifySessionAccept(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request) {
    if (envelope.type != SessionMessageType::accept)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence != 1 || !envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.toDeviceId ||
        envelope.toDeviceId != request.envelope.fromDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.targetPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionAcceptPayload(envelope.signedPayload, request.payload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (!envelope.sessionEpoch || *envelope.sessionEpoch != payload->sessionEpoch ||
        envelope.expiresAt != payload->expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionAccept{.envelope = envelope, .payload = std::move(*payload)};
}

SessionRejectVerificationResult verifySessionReject(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request) {
    if (envelope.type != SessionMessageType::reject)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence != 1 || envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.toDeviceId ||
        envelope.toDeviceId != request.envelope.fromDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.targetPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionRejectPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->requestDigest != sessionPayloadDigest(request.envelope.signedPayload) ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionReject{.envelope = envelope, .payload = std::move(*payload)};
}

SessionCancelVerificationResult verifySessionCancel(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request) {
    if (envelope.type != SessionMessageType::cancel)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence != 1 || envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.fromDeviceId ||
        envelope.toDeviceId != request.envelope.toDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.requesterPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionCancelPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->requestDigest != sessionPayloadDigest(request.envelope.signedPayload) ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionCancel{.envelope = envelope, .payload = std::move(*payload)};
}

SessionOfferVerificationResult verifySessionOffer(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept) {
    if (envelope.type != SessionMessageType::offer)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence < 2 || !envelope.sessionEpoch || !envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.fromDeviceId ||
        envelope.toDeviceId != request.envelope.toDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId ||
        *envelope.sessionEpoch != accept.payload.sessionEpoch) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.requesterPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionOfferPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->acceptDigest != sessionPayloadDigest(accept.envelope.signedPayload) ||
        payload->iceGeneration != *envelope.iceGeneration ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionOffer{.envelope = envelope, .payload = std::move(*payload)};
}

SessionAnswerVerificationResult verifySessionAnswer(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer) {
    if (envelope.type != SessionMessageType::answer)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence < 2 || !envelope.sessionEpoch || !envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.toDeviceId ||
        envelope.toDeviceId != request.envelope.fromDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId ||
        *envelope.sessionEpoch != accept.payload.sessionEpoch) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.targetPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionAnswerPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->offerDigest != sessionPayloadDigest(offer.envelope.signedPayload) ||
        payload->iceGeneration != *envelope.iceGeneration ||
        payload->iceGeneration != offer.payload.iceGeneration ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionAnswer{.envelope = envelope, .payload = std::move(*payload)};
}

SessionCandidatesVerificationResult verifySessionCandidates(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer,
    const VerifiedSessionAnswer* answer) {
    if (envelope.type != SessionMessageType::candidates)
        return SessionVerificationError::wrongMessageType;

    const auto binding = sessionSignalBinding(envelope, request, accept, offer, answer);
    if (!binding || envelope.sequence <= binding->descriptionSequence ||
        *envelope.iceGeneration != binding->iceGeneration) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(*binding->signerPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionCandidatesPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->descriptionDigest != binding->descriptionDigest ||
        payload->iceGeneration != binding->iceGeneration ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionCandidates{.envelope = envelope, .payload = std::move(*payload)};
}

SessionGatheringCompleteVerificationResult verifySessionGatheringComplete(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer,
    const VerifiedSessionAnswer* answer) {
    if (envelope.type != SessionMessageType::gatheringComplete)
        return SessionVerificationError::wrongMessageType;

    const auto binding = sessionSignalBinding(envelope, request, accept, offer, answer);
    if (!binding || envelope.sequence <= binding->descriptionSequence ||
        *envelope.iceGeneration != binding->iceGeneration) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(*binding->signerPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionGatheringCompletePayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    if (payload->descriptionDigest != binding->descriptionDigest ||
        payload->iceGeneration != binding->iceGeneration ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionGatheringComplete{
        .envelope = envelope,
        .payload = std::move(*payload),
    };
}

SessionEndVerificationResult verifySessionEnd(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionRevoke* previousRevoke) {
    if (envelope.type != SessionMessageType::end)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence < 2 || !envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId ||
        *envelope.sessionEpoch != accept.payload.sessionEpoch) {
        return SessionVerificationError::invalidRoute;
    }

    const SessionPublicKey* signerPublicKey = nullptr;
    if (envelope.fromDeviceId == request.envelope.fromDeviceId &&
        envelope.toDeviceId == request.envelope.toDeviceId) {
        signerPublicKey = &request.payload.requesterPublicKey;
    } else if (envelope.fromDeviceId == request.envelope.toDeviceId &&
               envelope.toDeviceId == request.envelope.fromDeviceId) {
        signerPublicKey = &request.payload.targetPublicKey;
    } else {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(*signerPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionEndPayload(envelope.signedPayload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    const auto& authorizationPayload = previousRevoke
                                           ? previousRevoke->envelope.signedPayload
                                           : accept.envelope.signedPayload;
    if (payload->authorizationDigest != sessionPayloadDigest(authorizationPayload) ||
        payload->sessionEpoch != accept.payload.sessionEpoch ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionEnd{.envelope = envelope, .payload = std::move(*payload)};
}

SessionRevokeVerificationResult verifySessionRevoke(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionRevoke* previousRevoke) {
    if (envelope.type != SessionMessageType::revoke)
        return SessionVerificationError::wrongMessageType;
    if (envelope.sequence < 2 || !envelope.sessionEpoch || envelope.iceGeneration ||
        envelope.fromDeviceId != request.envelope.toDeviceId ||
        envelope.toDeviceId != request.envelope.fromDeviceId ||
        envelope.requestId != request.envelope.requestId ||
        envelope.sessionId != request.envelope.sessionId ||
        *envelope.sessionEpoch != accept.payload.sessionEpoch) {
        return SessionVerificationError::invalidRoute;
    }
    if (!security::verifyEd25519(request.payload.targetPublicKey, envelope.signedPayload,
                                 envelope.signature)) {
        return SessionVerificationError::invalidSignature;
    }

    auto payload = parseSessionRevokePayload(envelope.signedPayload, request.payload);
    if (!payload)
        return SessionVerificationError::invalidPayload;
    const auto& authorizationPayload = previousRevoke
                                           ? previousRevoke->envelope.signedPayload
                                           : accept.envelope.signedPayload;
    const auto& currentGrants = previousRevoke
                                    ? previousRevoke->payload.grantedGrants
                                    : accept.payload.grantedGrants;
    const std::uint64_t currentPermissionEpoch = previousRevoke
                                                     ? previousRevoke->payload.permissionEpoch
                                                     : accept.payload.permissionEpoch;
    if (payload->authorizationDigest != sessionPayloadDigest(authorizationPayload) ||
        payload->grantedGrants.size() >= currentGrants.size() ||
        !capabilityGrantsAreSubset(payload->grantedGrants, currentGrants) ||
        payload->sessionEpoch != accept.payload.sessionEpoch ||
        payload->permissionEpoch <= currentPermissionEpoch ||
        payload->sequence != envelope.sequence || payload->expiresAt != envelope.expiresAt) {
        return SessionVerificationError::envelopeMismatch;
    }
    return VerifiedSessionRevoke{.envelope = envelope, .payload = std::move(*payload)};
}

}  // namespace easydesk::protocol
