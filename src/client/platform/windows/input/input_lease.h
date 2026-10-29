#pragma once

#include <QByteArray>

#include <chrono>

class InputLease final {
  public:
    using Clock = std::chrono::steady_clock;

    bool begin(const QByteArray &leaseId, quint64 generation, int timeoutMilliseconds);
    bool renew(const QByteArray &leaseId, quint64 generation, quint64 sequence,
               const QByteArray &challenge, Clock::time_point now);
    bool expireIfNeeded(Clock::time_point now);
    void invalidate();
    void reset();

    bool active(Clock::time_point now) const;
    bool matches(const QByteArray &leaseId) const;
    QByteArray challenge() const;

  private:
    QByteArray m_leaseId;
    QByteArray m_challenge;
    quint64 m_generation = 0;
    quint64 m_lastSequence = 0;
    int m_timeoutMilliseconds = 0;
    Clock::time_point m_deadline{};
    bool m_started = false;
    bool m_active = false;

    static QByteArray createChallenge();
};
