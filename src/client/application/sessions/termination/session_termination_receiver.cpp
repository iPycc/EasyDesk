#include "sessions/termination/session_termination_receiver.h"

#include "media/webrtc/media_runtime_library.h"
#include "network/signaling/protocol/session/termination/session_end_decoder.h"

#include <QDateTime>

namespace {

constexpr qint64 kMaximumEndLifetimeSeconds = 45;

}  // namespace

SessionTerminationReceiver::SessionTerminationReceiver(
    MediaRuntimeLibrary &media,
    QObject *parent)
    : QObject(parent), m_media(media) {}

bool SessionTerminationReceiver::begin(
    const SessionNegotiationContext &context,
    QString &error) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_active || !context.isValid() || context.expiresAt <= now ||
        context.signedAcceptPayload.isEmpty()) {
        error = QStringLiteral("Session termination context is invalid.");
        return false;
    }
    m_context = context;
    m_lastRemoteSequence = 1;
    m_active = true;
    error.clear();
    return true;
}

bool SessionTerminationReceiver::noteRemoteSequence(quint64 sequence,
                                                    QString &error) {
    if (!m_active || sequence <= m_lastRemoteSequence) {
        error = QStringLiteral("Remote session sequence is not increasing.");
        return false;
    }
    m_lastRemoteSequence = sequence;
    error.clear();
    return true;
}

void SessionTerminationReceiver::stop() {
    m_context = {};
    m_lastRemoteSequence = 0;
    m_active = false;
}

bool SessionTerminationReceiver::isActive() const {
    return m_active;
}

void SessionTerminationReceiver::handleEnd(const QByteArray &message) {
    if (!m_active) {
        fail(QStringLiteral("E_REMOTE_SESSION_END"),
             QStringLiteral("Remote session termination is not active."));
        return;
    }

    easydesk::client::signaling::SessionEndContext context;
    context.requestId = m_context.requestId;
    context.sessionId = m_context.sessionId;
    context.authorizationPayload = m_context.signedAcceptPayload;
    context.fromDeviceId = m_context.peerDeviceId;
    context.toDeviceId = m_context.localDeviceId;
    context.sessionEpoch = m_context.sessionEpoch;

    easydesk::client::signaling::DecodedSessionEnd decoded;
    QString error;
    if (!easydesk::client::signaling::decodeSessionEndEnvelope(
            message, context, decoded, error)) {
        fail(QStringLiteral("E_REMOTE_SESSION_END_PROTOCOL"), error);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (decoded.sequence != m_lastRemoteSequence + 1 ||
        decoded.expiresAt < now ||
        decoded.expiresAt > now + kMaximumEndLifetimeSeconds ||
        decoded.expiresAt > m_context.expiresAt) {
        fail(QStringLiteral("E_REMOTE_SESSION_END_SEQUENCE"),
             QStringLiteral(
                 "Remote session termination sequence or lifetime is invalid."));
        return;
    }
    if (!m_media.verifyEd25519(m_context.peerPublicKey,
                               decoded.signedPayload,
                               decoded.signature)) {
        fail(QStringLiteral("E_REMOTE_SESSION_END_SIGNATURE"),
             m_media.errorString());
        return;
    }

    m_lastRemoteSequence = decoded.sequence;
    m_active = false;
    emit ended(decoded.reason, decoded.sequence);
}

void SessionTerminationReceiver::fail(const QString &code,
                                      const QString &message) {
    stop();
    emit failed(code, message);
}
