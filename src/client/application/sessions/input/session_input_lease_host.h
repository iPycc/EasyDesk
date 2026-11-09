#pragma once

#include "features/sessions/model/session_state.h"
#include "network/input/protocol/lease/input_lease_codec.h"

#include <QRect>

class WindowsInputController;

class SessionInputLeaseHost final {
  public:
    explicit SessionInputLeaseHost(WindowsInputController &input);

    bool start(const SessionState &state, const QRect &displayRect,
               quint64 displayGeneration, quint64 leaseGeneration,
               QByteArray &challenge, QString &error);
    bool renew(const QByteArray &message, QByteArray &nextChallenge, QString &error);
    void stop();

  private:
    bool makeChallenge(quint64 acknowledgedSequence, QByteArray &output, QString &error);

    WindowsInputController &m_input;
    InputCommandContext m_context;
    quint64 m_leaseGeneration = 0;
    QByteArray m_previousSessionId;
    quint64 m_previousSessionEpoch = 0;
    quint64 m_lastLeaseGeneration = 0;
};
