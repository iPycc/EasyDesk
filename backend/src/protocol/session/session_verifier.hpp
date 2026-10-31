#pragma once

#include "protocol/session/session_accept.hpp"
#include "protocol/session/session_candidate.hpp"
#include "protocol/session/session_decision.hpp"
#include "protocol/session/session_description.hpp"
#include "protocol/session/session_envelope.hpp"
#include "protocol/session/session_request.hpp"
#include "protocol/session/session_termination.hpp"

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

struct VerifiedSessionReject {
    SessionEnvelope envelope;
    SessionRejectPayload payload;
};

struct VerifiedSessionCancel {
    SessionEnvelope envelope;
    SessionCancelPayload payload;
};

struct VerifiedSessionOffer {
    SessionEnvelope envelope;
    SessionOfferPayload payload;
};

struct VerifiedSessionAnswer {
    SessionEnvelope envelope;
    SessionAnswerPayload payload;
};

struct VerifiedSessionCandidates {
    SessionEnvelope envelope;
    SessionCandidatesPayload payload;
};

struct VerifiedSessionGatheringComplete {
    SessionEnvelope envelope;
    SessionGatheringCompletePayload payload;
};

struct VerifiedSessionEnd {
    SessionEnvelope envelope;
    SessionEndPayload payload;
};

struct VerifiedSessionRevoke {
    SessionEnvelope envelope;
    SessionRevokePayload payload;
};

using SessionRequestVerificationResult =
    std::variant<VerifiedSessionRequest, SessionVerificationError>;
using SessionAcceptVerificationResult =
    std::variant<VerifiedSessionAccept, SessionVerificationError>;
using SessionRejectVerificationResult =
    std::variant<VerifiedSessionReject, SessionVerificationError>;
using SessionCancelVerificationResult =
    std::variant<VerifiedSessionCancel, SessionVerificationError>;
using SessionOfferVerificationResult =
    std::variant<VerifiedSessionOffer, SessionVerificationError>;
using SessionAnswerVerificationResult =
    std::variant<VerifiedSessionAnswer, SessionVerificationError>;
using SessionCandidatesVerificationResult =
    std::variant<VerifiedSessionCandidates, SessionVerificationError>;
using SessionGatheringCompleteVerificationResult =
    std::variant<VerifiedSessionGatheringComplete, SessionVerificationError>;
using SessionEndVerificationResult =
    std::variant<VerifiedSessionEnd, SessionVerificationError>;
using SessionRevokeVerificationResult =
    std::variant<VerifiedSessionRevoke, SessionVerificationError>;

SessionRequestVerificationResult verifySessionRequest(
    const SessionEnvelope& envelope,
    const SessionPublicKey& registeredSourcePublicKey,
    const SessionPublicKey& registeredTargetPublicKey);
SessionAcceptVerificationResult verifySessionAccept(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request);
SessionRejectVerificationResult verifySessionReject(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request);
SessionCancelVerificationResult verifySessionCancel(
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
SessionCandidatesVerificationResult verifySessionCandidates(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer,
    const VerifiedSessionAnswer* answer);
SessionGatheringCompleteVerificationResult verifySessionGatheringComplete(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionOffer& offer,
    const VerifiedSessionAnswer* answer);
SessionEndVerificationResult verifySessionEnd(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionRevoke* previousRevoke);
SessionRevokeVerificationResult verifySessionRevoke(
    const SessionEnvelope& envelope,
    const VerifiedSessionRequest& request,
    const VerifiedSessionAccept& accept,
    const VerifiedSessionRevoke* previousRevoke);

}  // namespace easydesk::protocol
