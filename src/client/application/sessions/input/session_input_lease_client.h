#pragma once

#include "features/sessions/model/session_state.h"
#include "network/input/protocol/lease/input_lease_codec.h"

#include <QElapsedTimer>

class SessionInputLeaseClient final {
  public:
    bool start(const SessionState &state, quint64 displayGeneration, QString &error);
    bool acceptChallenge(const QByteArray &message, QString &error);
    bool makeRenewal(quint64 reliableSequence, QByteArray &message, QString &error);
    void stop();
    bool hasPendingChallenge() const;
    bool commandContext(InputStream stream, quint64 sequence, InputCommandContext &context) const;

  private:
    InputCommandContext m_context;
    QByteArray m_challenge;
    quint64 m_leaseGeneration = 0;
    quint64 m_lastRenewalSequence = 0;
    quint32 m_timeoutMilliseconds = 0;
    QElapsedTimer m_challengeAge;
    QElapsedTimer m_lastRenewal;
    QByteArray m_previousSessionId;
    quint64 m_previousSessionEpoch = 0;
    quint64 m_lastLeaseGeneration = 0;
};
