#include "input/input_lease.h"

#include <windows.h>
#include <bcrypt.h>

#include <utility>

namespace {
constexpr int kLeaseTokenBytes = 16;
constexpr int kMinimumLeaseMilliseconds = 1;
constexpr int kMaximumLeaseMilliseconds = 1500;
} // namespace

bool InputLease::begin(const QByteArray &leaseId, quint64 generation, int timeoutMilliseconds) {
    if (leaseId.size() != kLeaseTokenBytes || generation <= m_generation ||
        timeoutMilliseconds < kMinimumLeaseMilliseconds ||
        timeoutMilliseconds > kMaximumLeaseMilliseconds)
        return false;

    QByteArray nextChallenge = createChallenge();
    if (nextChallenge.size() != kLeaseTokenBytes)
        return false;

    m_leaseId = leaseId;
    m_generation = generation;
    m_lastSequence = 0;
    m_timeoutMilliseconds = timeoutMilliseconds;
    m_deadline = Clock::now() + std::chrono::milliseconds(m_timeoutMilliseconds);
    m_challenge = std::move(nextChallenge);
    m_started = true;
    m_active = false;
    return true;
}

bool InputLease::renew(const QByteArray &leaseId, quint64 generation, quint64 sequence,
                       const QByteArray &challenge, Clock::time_point now) {
    if (!m_started || leaseId != m_leaseId || generation != m_generation || sequence == 0 ||
        sequence <= m_lastSequence || challenge != m_challenge)
        return false;
    if (now >= m_deadline) {
        invalidate();
        return false;
    }

    QByteArray nextChallenge = createChallenge();
    if (nextChallenge.size() != kLeaseTokenBytes)
        return false;
    m_challenge = std::move(nextChallenge);
    m_lastSequence = sequence;
    m_deadline = now + std::chrono::milliseconds(m_timeoutMilliseconds);
    m_active = true;
    return true;
}

bool InputLease::expireIfNeeded(Clock::time_point now) {
    if (!m_started || now < m_deadline)
        return false;
    invalidate();
    return true;
}

void InputLease::invalidate() {
    m_leaseId.clear();
    m_challenge.clear();
    m_lastSequence = 0;
    m_timeoutMilliseconds = 0;
    m_deadline = {};
    m_started = false;
    m_active = false;
}

void InputLease::reset() {
    invalidate();
    m_generation = 0;
}

bool InputLease::active(Clock::time_point now) const {
    return m_active && now < m_deadline;
}

bool InputLease::matches(const QByteArray &leaseId) const {
    return m_started && leaseId == m_leaseId;
}

QByteArray InputLease::challenge() const {
    return m_challenge;
}

QByteArray InputLease::createChallenge() {
    QByteArray bytes(kLeaseTokenBytes, Qt::Uninitialized);
    const NTSTATUS status = BCryptGenRandom(
        nullptr, reinterpret_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0)
        return {};
    return bytes;
}
