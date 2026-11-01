#include "sessions/negotiation/ice/session_ice_signal_sender.h"

#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/session/ice/session_ice_codec.h"

#include <QDateTime>

namespace {

constexpr qint64 kCandidateLifetimeSeconds = 45;

}  // namespace

SessionIceSignalSender::SessionIceSignalSender(
    WindowsIdentityStore &identity,
    SignalingClient &signaling,
    QObject *parent)
    : QObject(parent), m_identity(identity), m_signaling(signaling) {
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(0);
    connect(&m_flushTimer, &QTimer::timeout,
            this, &SessionIceSignalSender::flushCandidates);
}

bool SessionIceSignalSender::begin(const SessionNegotiationContext &context,
                                   quint32 iceGeneration,
                                   QString &error) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_active || !context.isValid() || context.expiresAt <= now ||
        iceGeneration == 0) {
        error = QStringLiteral("Session ICE sender context is invalid.");
        return false;
    }
    m_context = context;
    m_iceGeneration = iceGeneration;
    m_lastSequence = 0;
    m_active = true;
    error.clear();
    return true;
}

bool SessionIceSignalSender::bindLocalDescription(
    const QByteArray &signedDescriptionPayload,
    quint64 descriptionSequence,
    QString &error) {
    if (!m_active || signedDescriptionPayload.isEmpty() ||
        descriptionSequence < 2 || !m_signedDescriptionPayload.isEmpty()) {
        error = QStringLiteral("Local ICE description binding is invalid.");
        return false;
    }
    m_signedDescriptionPayload = signedDescriptionPayload;
    m_lastSequence = descriptionSequence;
    scheduleFlush();
    error.clear();
    return true;
}

void SessionIceSignalSender::stop() {
    m_flushTimer.stop();
    m_context = {};
    m_buffer.clear();
    m_signedDescriptionPayload.clear();
    m_iceGeneration = 0;
    m_lastSequence = 0;
    m_active = false;
}

bool SessionIceSignalSender::isActive() const {
    return m_active;
}

void SessionIceSignalSender::enqueueCandidate(
    quint32 iceGeneration,
    const QByteArray &sdpMid,
    qint32 sdpMlineIndex,
    const QByteArray &candidate) {
    if (!m_active)
        return;
    QString error;
    if (!m_buffer.enqueue(iceGeneration, sdpMid, sdpMlineIndex,
                          candidate, error)) {
        fail(QStringLiteral("E_LOCAL_CANDIDATE"), error);
        return;
    }
    scheduleFlush();
}

void SessionIceSignalSender::scheduleFlush() {
    if (m_active && !m_signedDescriptionPayload.isEmpty() &&
        !m_buffer.isEmpty() && !m_flushTimer.isActive()) {
        m_flushTimer.start();
    }
}

void SessionIceSignalSender::flushCandidates() {
    if (!m_active || m_signedDescriptionPayload.isEmpty() ||
        m_buffer.isEmpty()) {
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 expiresAt =
        qMin(now + kCandidateLifetimeSeconds, m_context.expiresAt);
    if (expiresAt <= now) {
        fail(QStringLiteral("E_CANDIDATE_EXPIRED"),
             QStringLiteral("Session ICE candidate lifetime expired."));
        return;
    }

    easydesk::client::signaling::SessionCandidatesFields fields;
    fields.route.requestId = m_context.requestId;
    fields.route.sessionId = m_context.sessionId;
    fields.route.sessionEpoch = m_context.sessionEpoch;
    fields.route.iceGeneration = m_iceGeneration;
    fields.route.sequence = m_lastSequence + 1;
    fields.route.expiresAt = expiresAt;
    fields.route.fromDeviceId = m_context.localDeviceId;
    fields.route.toDeviceId = m_context.peerDeviceId;
    fields.signedDescriptionPayload = m_signedDescriptionPayload;
    fields.candidates = m_buffer.takeBatch();

    QString error;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionCandidatesSigningBytes(fields,
                                                                   error);
    if (signingBytes.isEmpty()) {
        fail(QStringLiteral("E_CANDIDATE_PROTOCOL"), error);
        return;
    }
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signature.isEmpty()) {
        fail(QStringLiteral("E_CANDIDATE_SIGNATURE"),
             m_identity.errorString());
        return;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionCandidatesEnvelope(
            fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        fail(QStringLiteral("E_CANDIDATE_SEND"),
             error.isEmpty()
                 ? QStringLiteral("Session ICE candidates could not be sent.")
                 : error);
        return;
    }

    m_lastSequence = fields.route.sequence;
    emit candidatesSent(fields.candidates.size(), m_lastSequence);
    scheduleFlush();
}

void SessionIceSignalSender::fail(const QString &code,
                                  const QString &message) {
    stop();
    emit failed(code, message);
}
