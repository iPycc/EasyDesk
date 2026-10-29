#include "features/sessions/model/session_state.h"

namespace {
bool isTerminal(SessionPhase phase) {
    return phase == SessionPhase::Ended || phase == SessionPhase::Failed;
}
} // namespace

bool isSessionTransitionAllowed(SessionPhase from, SessionPhase to) {
    if (from == to)
        return true;

    switch (from) {
    case SessionPhase::Idle:
        return to == SessionPhase::AwaitingApproval;
    case SessionPhase::AwaitingApproval:
        return to == SessionPhase::Negotiating || to == SessionPhase::Stopping ||
               to == SessionPhase::Ended || to == SessionPhase::Failed;
    case SessionPhase::Negotiating:
        return to == SessionPhase::Connecting || to == SessionPhase::Stopping ||
               to == SessionPhase::Failed;
    case SessionPhase::Connecting:
        return to == SessionPhase::Active || to == SessionPhase::Recovering ||
               to == SessionPhase::Stopping || to == SessionPhase::Failed;
    case SessionPhase::Active:
        return to == SessionPhase::Recovering || to == SessionPhase::Stopping ||
               to == SessionPhase::Failed;
    case SessionPhase::Recovering:
        return to == SessionPhase::Connecting || to == SessionPhase::Active ||
               to == SessionPhase::Stopping || to == SessionPhase::Failed;
    case SessionPhase::Stopping:
        return to == SessionPhase::Ended || to == SessionPhase::Failed;
    case SessionPhase::Ended:
    case SessionPhase::Failed:
        return to == SessionPhase::Idle;
    }
    return false;
}

bool SessionState::transitionTo(SessionPhase next) {
    if (!isSessionTransitionAllowed(phase, next))
        return false;
    phase = next;
    if (next == SessionPhase::Stopping || isTerminal(next)) {
        access.revokeAll();
        readiness = {};
    }
    if (next != SessionPhase::Failed)
        failureReason.clear();
    return true;
}

void SessionState::fail(const QString &reason) {
    if (isSessionTransitionAllowed(phase, SessionPhase::Failed)) {
        phase = SessionPhase::Failed;
        access.revokeAll();
        readiness = {};
    }
    failureReason = reason;
}

void SessionState::reset() {
    sessionId.clear();
    peerDeviceId.clear();
    sessionEpoch = 0;
    permissionEpoch = 0;
    role = SessionRole::Controller;
    phase = SessionPhase::Idle;
    access = {};
    readiness = {};
    failureReason.clear();
}

bool SessionState::authenticated() const {
    return readiness.peerIdentityVerified && readiness.dtlsConnected && readiness.sessionReady;
}

bool SessionState::canPresentDesktop() const {
    return phase == SessionPhase::Active && authenticated() &&
           access.allows(SessionCapability::ViewDesktop) && readiness.firstFramePresented;
}

bool SessionState::canUseInput() const {
    return phase == SessionPhase::Active && authenticated() &&
           access.allows(SessionCapability::ControlInput) && readiness.inputChannelReady &&
           readiness.inputLeaseActive;
}
