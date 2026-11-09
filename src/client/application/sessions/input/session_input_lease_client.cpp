#include "sessions/input/session_input_lease_client.h"

namespace {
constexpr int kMinimumRenewalIntervalMilliseconds = 100;
}

bool SessionInputLeaseClient::start(const SessionState &state, quint64 displayGeneration,
                                    QString &error) {
    if (!m_context.sessionId.isEmpty() || state.role != SessionRole::Controller ||
        state.phase != SessionPhase::Active || !state.authenticated() ||
        !state.access.allows(SessionCapability::ControlInput) ||
        !state.readiness.firstFramePresented || !state.readiness.inputChannelReady ||
        state.sessionId.size() != 16 || state.sessionEpoch == 0 ||
        state.permissionEpoch == 0 || displayGeneration == 0) {
        error = QStringLiteral("Input session is not authorized or ready.");
        return false;
    }
    m_context.sessionId = state.sessionId;
    m_context.sessionEpoch = state.sessionEpoch;
    m_context.permissionEpoch = state.permissionEpoch;
    m_context.displayGeneration = displayGeneration;
    m_context.stream = InputStream::Reliable;
    error.clear();
    return true;
}

bool SessionInputLeaseClient::acceptChallenge(const QByteArray &message, QString &error) {
    easydesk::client::input::InputLeaseMessage challenge;
    if (m_context.sessionId.isEmpty() || !m_challenge.isEmpty() ||
        !easydesk::client::input::decodeInputLeaseMessage(message, challenge, error) ||
        challenge.kind != easydesk::client::input::InputLeaseMessageKind::Challenge ||
        challenge.context.sessionId != m_context.sessionId ||
        challenge.context.sessionEpoch != m_context.sessionEpoch ||
        challenge.context.permissionEpoch != m_context.permissionEpoch ||
        challenge.context.displayGeneration != m_context.displayGeneration) {
        error = QStringLiteral("Input lease challenge does not match the active session.");
        return false;
    }
    if (m_leaseGeneration == 0) {
        if (challenge.context.sequence != 0 ||
            (m_context.sessionId == m_previousSessionId &&
             m_context.sessionEpoch == m_previousSessionEpoch &&
             challenge.leaseGeneration <= m_lastLeaseGeneration)) {
            error = QStringLiteral("Input lease challenge generation is stale.");
            return false;
        }
        m_context.leaseId = challenge.context.leaseId;
        m_leaseGeneration = challenge.leaseGeneration;
        m_previousSessionId = m_context.sessionId;
        m_previousSessionEpoch = m_context.sessionEpoch;
        m_lastLeaseGeneration = m_leaseGeneration;
    } else if (challenge.context.leaseId != m_context.leaseId ||
               challenge.leaseGeneration != m_leaseGeneration ||
               challenge.context.sequence != m_lastRenewalSequence ||
               m_lastRenewalSequence == 0) {
        error = QStringLiteral("Input lease challenge is out of sequence.");
        return false;
    }
    m_challenge = challenge.challenge;
    m_timeoutMilliseconds = challenge.timeoutMilliseconds;
    m_challengeAge.start();
    error.clear();
    return true;
}

bool SessionInputLeaseClient::makeRenewal(quint64 reliableSequence, QByteArray &message,
                                          QString &error) {
    message.clear();
    if (!hasPendingChallenge() || reliableSequence == 0 ||
        reliableSequence <= m_lastRenewalSequence ||
        (m_lastRenewal.isValid() &&
         m_lastRenewal.elapsed() < kMinimumRenewalIntervalMilliseconds)) {
        error = QStringLiteral("Input lease renewal is unavailable or too early.");
        return false;
    }
    easydesk::client::input::InputLeaseMessage renewal;
    renewal.kind = easydesk::client::input::InputLeaseMessageKind::Renewal;
    renewal.context = m_context;
    renewal.context.sequence = reliableSequence;
    renewal.leaseGeneration = m_leaseGeneration;
    renewal.challenge = m_challenge;
    if (!easydesk::client::input::encodeInputLeaseMessage(renewal, message, error))
        return false;
    m_challenge.clear();
    m_challengeAge.invalidate();
    m_lastRenewalSequence = reliableSequence;
    m_lastRenewal.start();
    return true;
}

void SessionInputLeaseClient::stop() {
    m_context = {};
    m_challenge.clear();
    m_leaseGeneration = 0;
    m_lastRenewalSequence = 0;
    m_timeoutMilliseconds = 0;
    m_challengeAge.invalidate();
    m_lastRenewal.invalidate();
}

bool SessionInputLeaseClient::hasPendingChallenge() const {
    return !m_challenge.isEmpty() && m_challengeAge.isValid() &&
           m_challengeAge.elapsed() < m_timeoutMilliseconds;
}

bool SessionInputLeaseClient::commandContext(InputStream stream, quint64 sequence,
                                             InputCommandContext &context) const {
    context = {};
    if ((stream != InputStream::Reliable && stream != InputStream::Pointer) ||
        sequence == 0 || m_leaseGeneration == 0 || m_context.leaseId.size() != 16)
        return false;
    context = m_context;
    context.stream = stream;
    context.sequence = sequence;
    return true;
}
