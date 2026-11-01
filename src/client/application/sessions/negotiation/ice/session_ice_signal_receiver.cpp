#include "sessions/negotiation/ice/session_ice_signal_receiver.h"

#include "media/webrtc/media_runtime_library.h"
#include "media/webrtc/media_session.h"
#include "network/signaling/protocol/session/ice/session_ice_decoder.h"

#include <QDateTime>

#include <utility>

namespace {

constexpr qint64 kMaximumCandidateLifetimeSeconds = 45;
constexpr int kMaximumRemoteCandidates = 64;

bool sameCandidate(
    const easydesk::client::signaling::SessionIceCandidate &left,
    const easydesk::client::signaling::SessionIceCandidate &right) {
    return left.sdpMid == right.sdpMid &&
           left.sdpMLineIndex == right.sdpMLineIndex &&
           left.usernameFragment == right.usernameFragment &&
           left.candidate == right.candidate;
}

}  // namespace

SessionIceSignalReceiver::SessionIceSignalReceiver(MediaRuntimeLibrary &media,
                                                   QObject *parent)
    : QObject(parent), m_media(media) {}

bool SessionIceSignalReceiver::begin(
    const SessionNegotiationContext &context,
    MediaSession &session,
    quint32 iceGeneration,
    QString &error) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_active || !context.isValid() || context.expiresAt <= now ||
        !session.isActive() || iceGeneration == 0) {
        error = QStringLiteral("Session ICE receiver context is invalid.");
        return false;
    }
    m_context = context;
    m_mediaSession = &session;
    m_iceGeneration = iceGeneration;
    m_active = true;
    error.clear();
    return true;
}

bool SessionIceSignalReceiver::bindRemoteDescription(
    const QByteArray &signedDescriptionPayload,
    quint64 descriptionSequence,
    const QByteArray &sdp,
    QString &error) {
    if (!m_active || signedDescriptionPayload.isEmpty() ||
        descriptionSequence < 2 || !m_signedDescriptionPayload.isEmpty() ||
        !m_descriptionBinding.parse(sdp, error)) {
        if (error.isEmpty())
            error = QStringLiteral("Remote ICE description binding is invalid.");
        return false;
    }
    m_signedDescriptionPayload = signedDescriptionPayload;
    m_descriptionSequence = descriptionSequence;
    m_lastSequence = descriptionSequence;
    error.clear();
    return true;
}

bool SessionIceSignalReceiver::markRemoteDescriptionApplied(
    quint32 iceGeneration,
    QString &error) {
    if (!m_active || !m_mediaSession ||
        m_signedDescriptionPayload.isEmpty() ||
        iceGeneration != m_iceGeneration || m_remoteDescriptionApplied) {
        error = QStringLiteral(
            "Remote ICE description confirmation is invalid.");
        return false;
    }
    m_remoteDescriptionApplied = true;
    const QVector<PendingBatch> pending = std::move(m_pending);
    m_pending.clear();
    for (const PendingBatch &batch : pending) {
        if (!applyBatch(batch, error))
            return false;
    }
    error.clear();
    return true;
}

void SessionIceSignalReceiver::stop() {
    m_mediaSession = nullptr;
    m_context = {};
    m_descriptionBinding.clear();
    m_pending.clear();
    m_seen.clear();
    m_signedDescriptionPayload.clear();
    m_iceGeneration = 0;
    m_descriptionSequence = 0;
    m_lastSequence = 0;
    m_remoteDescriptionApplied = false;
    m_active = false;
}

bool SessionIceSignalReceiver::isActive() const {
    return m_active;
}

void SessionIceSignalReceiver::handleCandidates(const QByteArray &message) {
    if (!m_active || !m_mediaSession ||
        m_signedDescriptionPayload.isEmpty()) {
        fail(QStringLiteral("E_REMOTE_CANDIDATE"),
             QStringLiteral("Remote ICE candidate context is invalid."));
        return;
    }

    easydesk::client::signaling::SessionIceContext context;
    context.requestId = m_context.requestId;
    context.sessionId = m_context.sessionId;
    context.sessionEpoch = m_context.sessionEpoch;
    context.iceGeneration = m_iceGeneration;
    context.descriptionSequence = m_descriptionSequence;
    context.fromDeviceId = m_context.peerDeviceId;
    context.toDeviceId = m_context.localDeviceId;
    context.signedDescriptionPayload = m_signedDescriptionPayload;

    easydesk::client::signaling::DecodedSessionCandidates decoded;
    QString error;
    if (!easydesk::client::signaling::decodeSessionCandidatesEnvelope(
            message, context, decoded, error)) {
        fail(QStringLiteral("E_REMOTE_CANDIDATE_PROTOCOL"), error);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (decoded.fields.route.sequence != m_lastSequence + 1 ||
        decoded.fields.route.expiresAt < now ||
        decoded.fields.route.expiresAt >
            now + kMaximumCandidateLifetimeSeconds ||
        decoded.fields.route.expiresAt > m_context.expiresAt) {
        fail(QStringLiteral("E_REMOTE_CANDIDATE_SEQUENCE"),
             QStringLiteral(
                 "Remote ICE candidate sequence or lifetime is invalid."));
        return;
    }
    if (!m_media.verifyEd25519(m_context.peerPublicKey,
                               decoded.signedPayload,
                               decoded.signature)) {
        fail(QStringLiteral("E_REMOTE_CANDIDATE_SIGNATURE"),
             m_media.errorString());
        return;
    }
    if (m_seen.size() + decoded.fields.candidates.size() >
        kMaximumRemoteCandidates) {
        fail(QStringLiteral("E_REMOTE_CANDIDATE_LIMIT"),
             QStringLiteral("Remote ICE candidate limit was exceeded."));
        return;
    }
    for (const auto &candidate : decoded.fields.candidates) {
        if (!m_descriptionBinding.matches(candidate, error) ||
            isDuplicate(candidate)) {
            fail(QStringLiteral("E_REMOTE_CANDIDATE_BINDING"),
                 error.isEmpty()
                     ? QStringLiteral("Remote ICE candidate is duplicated.")
                     : error);
            return;
        }
    }

    PendingBatch batch;
    batch.candidates = decoded.fields.candidates;
    batch.sequence = decoded.fields.route.sequence;
    for (const auto &candidate : batch.candidates)
        m_seen.append(candidate);
    m_lastSequence = batch.sequence;
    if (m_remoteDescriptionApplied) {
        if (!applyBatch(batch, error)) {
            fail(QStringLiteral("E_REMOTE_CANDIDATE_APPLY"), error);
            return;
        }
    } else {
        m_pending.append(std::move(batch));
    }
}

bool SessionIceSignalReceiver::applyBatch(const PendingBatch &batch,
                                          QString &error) {
    if (!m_mediaSession || !m_mediaSession->isActive()) {
        error = QStringLiteral("Remote ICE media session is unavailable.");
        return false;
    }
    for (const auto &candidate : batch.candidates) {
        if (!m_mediaSession->addIceCandidate(
                m_iceGeneration,
                candidate.sdpMid.toUtf8(),
                static_cast<qint32>(candidate.sdpMLineIndex),
                candidate.candidate.toUtf8())) {
            error = m_mediaSession->errorString();
            if (error.isEmpty())
                error = QStringLiteral("Remote ICE candidate was rejected.");
            return false;
        }
    }
    emit candidateBatchApplied(batch.candidates.size(), batch.sequence);
    error.clear();
    return true;
}

bool SessionIceSignalReceiver::isDuplicate(
    const easydesk::client::signaling::SessionIceCandidate &candidate) const {
    for (const auto &seen : m_seen) {
        if (sameCandidate(seen, candidate))
            return true;
    }
    return false;
}

void SessionIceSignalReceiver::fail(const QString &code,
                                    const QString &message) {
    stop();
    emit failed(code, message);
}
