#include "protocol/session/session_verifier.hpp"

#include "security/crypto/ed25519.hpp"

#include <utility>

namespace easydesk::protocol {

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

}  // namespace easydesk::protocol
