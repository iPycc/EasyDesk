#include "sessions/negotiation/ice/local_ice_candidate_buffer.h"

#include <QList>

#include <algorithm>
#include <utility>

namespace {

constexpr int kMaximumMidBytes = 64;
constexpr int kMaximumCandidateBytes = 2048;
constexpr int kMaximumUfragBytes = 256;
constexpr int kMandatoryCandidateTokenCount = 8;

bool decodeSingleLine(const QByteArray &bytes,
                      int maximumBytes,
                      QString &value) {
    if (bytes.isEmpty() || bytes.size() > maximumBytes ||
        bytes.contains('\0') || bytes.contains('\r') || bytes.contains('\n')) {
        return false;
    }
    value = QString::fromUtf8(bytes);
    return value.toUtf8() == bytes;
}

bool validUfrag(const QByteArray &value) {
    if (value.isEmpty() || value.size() > kMaximumUfragBytes)
        return false;
    for (const char byte : value) {
        const bool alphaNumeric =
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9');
        if (!alphaNumeric && byte != '+' && byte != '/')
            return false;
    }
    return true;
}

bool extractUsernameFragment(const QByteArray &candidate,
                             QString &usernameFragment) {
    const QList<QByteArray> tokens = candidate.split(' ');
    if (tokens.size() < kMandatoryCandidateTokenCount ||
        (tokens.size() - kMandatoryCandidateTokenCount) % 2 != 0) {
        return false;
    }

    QByteArray ufrag;
    for (int index = kMandatoryCandidateTokenCount;
         index < tokens.size(); index += 2) {
        if (tokens.at(index).isEmpty() || tokens.at(index + 1).isEmpty())
            return false;
        if (tokens.at(index) != QByteArrayLiteral("ufrag"))
            continue;
        if (!ufrag.isEmpty())
            return false;
        ufrag = tokens.at(index + 1);
    }
    if (!validUfrag(ufrag))
        return false;
    usernameFragment = QString::fromLatin1(ufrag);
    return true;
}

bool sameCandidate(
    const easydesk::client::signaling::SessionIceCandidate &left,
    const easydesk::client::signaling::SessionIceCandidate &right) {
    return left.sdpMid == right.sdpMid &&
           left.sdpMLineIndex == right.sdpMLineIndex &&
           left.usernameFragment == right.usernameFragment &&
           left.candidate == right.candidate;
}

}  // namespace

bool LocalIceCandidateBuffer::enqueue(quint32 iceGeneration,
                                      const QByteArray &sdpMid,
                                      qint32 sdpMlineIndex,
                                      const QByteArray &candidate,
                                      QString &error) {
    error.clear();
    if (iceGeneration == 0 || sdpMlineIndex < 0 || sdpMlineIndex > 255 ||
        (m_iceGeneration != 0 && m_iceGeneration != iceGeneration)) {
        error = QStringLiteral("Local ICE candidate generation is invalid.");
        return false;
    }

    QString decodedMid;
    QString decodedCandidate;
    QString usernameFragment;
    if (!decodeSingleLine(sdpMid, kMaximumMidBytes, decodedMid) ||
        !decodeSingleLine(candidate, kMaximumCandidateBytes,
                          decodedCandidate) ||
        !candidate.startsWith(QByteArrayLiteral("candidate:")) ||
        !extractUsernameFragment(candidate, usernameFragment)) {
        error = QStringLiteral("Local ICE candidate is invalid.");
        return false;
    }

    easydesk::client::signaling::SessionIceCandidate value;
    value.sdpMid = decodedMid;
    value.sdpMLineIndex = static_cast<quint32>(sdpMlineIndex);
    value.usernameFragment = usernameFragment;
    value.candidate = decodedCandidate;

    const auto duplicate = std::find_if(
        m_seen.cbegin(), m_seen.cend(),
        [&value](const auto &existing) {
            return sameCandidate(existing, value);
        });
    if (duplicate != m_seen.cend()) {
        error = QStringLiteral("Local ICE candidate is duplicated.");
        return false;
    }
    if (m_seen.size() >= kMaximumCandidatesPerGeneration) {
        error = QStringLiteral("Local ICE candidate limit was exceeded.");
        return false;
    }

    m_iceGeneration = iceGeneration;
    m_pending.append(value);
    m_seen.append(std::move(value));
    return true;
}

QVector<easydesk::client::signaling::SessionIceCandidate>
LocalIceCandidateBuffer::takeBatch() {
    const int count = qMin(m_pending.size(), kMaximumBatchSize);
    QVector<easydesk::client::signaling::SessionIceCandidate> batch;
    batch.reserve(count);
    for (int index = 0; index < count; ++index)
        batch.append(m_pending.at(index));
    m_pending.remove(0, count);
    return batch;
}

bool LocalIceCandidateBuffer::isEmpty() const {
    return m_pending.isEmpty();
}

int LocalIceCandidateBuffer::size() const {
    return m_pending.size();
}

quint32 LocalIceCandidateBuffer::iceGeneration() const {
    return m_iceGeneration;
}

void LocalIceCandidateBuffer::clear() {
    m_iceGeneration = 0;
    m_pending.clear();
    m_seen.clear();
}
