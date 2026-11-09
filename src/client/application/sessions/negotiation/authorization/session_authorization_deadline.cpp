#include "sessions/negotiation/authorization/session_authorization_deadline.h"

#include <QDateTime>

#include <limits>

namespace {
constexpr int kCheckIntervalMilliseconds = 250;
}

SessionAuthorizationDeadline::SessionAuthorizationDeadline(QObject *parent)
    : QObject(parent) {
    m_timer.setInterval(kCheckIntervalMilliseconds);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SessionAuthorizationDeadline::check);
}

bool SessionAuthorizationDeadline::start(qint64 expiresAt) {
    if (m_timer.isActive() || expiresAt <= 0 ||
        expiresAt > std::numeric_limits<qint64>::max() / 1000)
        return false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 deadline = expiresAt * 1000;
    if (now < 0 || deadline <= now)
        return false;
    m_expiresAt = expiresAt;
    m_remainingMilliseconds = deadline - now;
    m_elapsed.start();
    m_timer.start();
    return true;
}

void SessionAuthorizationDeadline::stop() {
    m_timer.stop();
    m_elapsed.invalidate();
    m_expiresAt = 0;
    m_remainingMilliseconds = 0;
}

void SessionAuthorizationDeadline::check() {
    if (!m_elapsed.isValid())
        return;
    if (QDateTime::currentSecsSinceEpoch() >= m_expiresAt ||
        m_elapsed.elapsed() >= m_remainingMilliseconds) {
        stop();
        emit expired();
    }
}
