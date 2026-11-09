#include "sessions/input/session_input_lease_host.h"

#include "input/windows_input_controller.h"
#include "security/secure_random.h"

namespace {
constexpr int kLeaseTimeoutMilliseconds = 750;
}

SessionInputLeaseHost::SessionInputLeaseHost(WindowsInputController &input) : m_input(input) {}

bool SessionInputLeaseHost::start(const SessionState &state, const QRect &displayRect,
                                  quint64 displayGeneration, quint64 leaseGeneration,
                                  QByteArray &challenge, QString &error) {
    challenge.clear();
    if (!m_context.sessionId.isEmpty() || leaseGeneration == 0 ||
        (state.sessionId == m_previousSessionId && state.sessionEpoch == m_previousSessionEpoch &&
         leaseGeneration <= m_lastLeaseGeneration)) {
        error = QStringLiteral("Input lease generation is invalid.");
        return false;
    }
    const QByteArray leaseId = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (leaseId.size() != 16)
        return false;
    if (!m_input.startSession(state, displayRect, displayGeneration)) {
        error = QStringLiteral("Input session is not authorized or ready.");
        return false;
    }
    if (!m_input.beginLease(leaseId, leaseGeneration, kLeaseTimeoutMilliseconds)) {
        m_input.stopSession();
        error = QStringLiteral("Input lease could not be created.");
        return false;
    }
    m_context.sessionId = state.sessionId;
    m_context.sessionEpoch = state.sessionEpoch;
    m_context.permissionEpoch = state.permissionEpoch;
    m_context.displayGeneration = displayGeneration;
    m_context.leaseId = leaseId;
    m_context.stream = InputStream::Reliable;
    m_leaseGeneration = leaseGeneration;
    m_previousSessionId = state.sessionId;
    m_previousSessionEpoch = state.sessionEpoch;
    m_lastLeaseGeneration = leaseGeneration;
    if (!makeChallenge(0, challenge, error)) {
        stop();
        return false;
    }
    return true;
}

bool SessionInputLeaseHost::renew(const QByteArray &message, QByteArray &nextChallenge,
                                  QString &error) {
    nextChallenge.clear();
    easydesk::client::input::InputLeaseMessage renewal;
    if (m_context.sessionId.isEmpty() ||
        !easydesk::client::input::decodeInputLeaseMessage(message, renewal, error) ||
        renewal.kind != easydesk::client::input::InputLeaseMessageKind::Renewal ||
        renewal.context.sessionId != m_context.sessionId ||
        renewal.context.sessionEpoch != m_context.sessionEpoch ||
        renewal.context.permissionEpoch != m_context.permissionEpoch ||
        renewal.context.displayGeneration != m_context.displayGeneration ||
        renewal.context.leaseId != m_context.leaseId ||
        renewal.leaseGeneration != m_leaseGeneration) {
        error = QStringLiteral("Input lease renewal does not match the active session.");
        return false;
    }
    if (!m_input.renewLease(renewal.context, renewal.leaseGeneration, renewal.challenge)) {
        error = QStringLiteral("Input lease renewal was rejected or expired.");
        return false;
    }
    if (!makeChallenge(renewal.context.sequence, nextChallenge, error)) {
        stop();
        return false;
    }
    return true;
}

void SessionInputLeaseHost::stop() {
    m_input.stopSession();
    m_context = {};
    m_leaseGeneration = 0;
}

bool SessionInputLeaseHost::makeChallenge(quint64 acknowledgedSequence,
                                          QByteArray &output, QString &error) {
    easydesk::client::input::InputLeaseMessage challenge;
    challenge.kind = easydesk::client::input::InputLeaseMessageKind::Challenge;
    challenge.context = m_context;
    challenge.context.sequence = acknowledgedSequence;
    challenge.leaseGeneration = m_leaseGeneration;
    challenge.challenge = m_input.leaseChallenge();
    challenge.timeoutMilliseconds = kLeaseTimeoutMilliseconds;
    return easydesk::client::input::encodeInputLeaseMessage(challenge, output, error);
}
