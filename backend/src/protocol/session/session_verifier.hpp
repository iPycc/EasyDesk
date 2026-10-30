#pragma once

#include "protocol/session/session_accept.hpp"
#include "protocol/session/session_description.hpp"
#include "protocol/session/session_envelope.hpp"
#include "protocol/session/session_request.hpp"

#include <variant>

namespace easydesk::protocol {

enum class SessionVerificationError {
    wrongMessageType,
    invalidRoute,
    invalidSignature,
    invalidPayload,
    envelopeMismatch,
};

struct VerifiedSessionRequest {
    SessionEnvelope envelope;
    SessionRequestPayload payload;
};

struct VerifiedSessionAccept {
    SessionEnvelope envelope;
    SessionAcceptPayload payload;
};

struct VerifiedSessionOffer {
    SessionEnvelope envelope;
    SessionOfferPayload payload;
};

struct VerifiedSessionAnswer {
    SessionEnvelope envelope;
    SessionAnswerPayload payload;
};

using SessionRequestVerificationResult =
    std::variant<VerifiedSessionRequest, SessionVerificationError>;
using SessionAcceptVerificationResult =
    std::variant<VerifiedSessionAccept, SessionVerificationError>;
using SessionOfferVerificationResult =
    std::variant<VerifiedSessionOffer, SessionVerificationError>;
using SessionAnswerVerificationResult =
    std::variant<VerifiedSessionAnswer, SessionVerificationError>;

SessionRequestVerificationResult verifySessionRequest(
    const SessionEnvelope& envelope,
    const SessionPublicKey& registeredSourcePublicKey,
    const SessionPublicKey& registeredTargetPublicKey);
SessionAcceptVerificationResult verifySessionAccept(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request);
SessionOfferVerificationResult verifySessionOffer(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept);
SessionAnswerVerificationResult verifySessionAnswer(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer);

}  // namespace easydesk::protocol
