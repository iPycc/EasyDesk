#include "sessions/termination/session_termination_sender.h"

#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/protocol/session/session_delivery_codec.h"

#include <QDateTime>

namespace {

constexpr int kDeliveryTimeoutMilliseconds = 15000;
constexpr qint64 kEndLifetimeSeconds = 45;

}  // namespace

SessionTerminationSender::SessionTerminationSender(
    WindowsIdentityStore &identity,
    SignalingClient &signaling,
    QObject *parent)
    : QObject(parent), m_identity(identity), m_signaling(signaling) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout,
            this, &SessionTerminationSender::handleTimeout);
}

bool SessionTerminationSender::begin(
    const SessionNegotiationContext &context,
    QString &error) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_state != State::Idle || !context.isValid() ||
        context.expiresAt <= now || context.signedAcceptPayload.isEmpty()) {
        error = QStringLiteral("Session termination sender context is invalid.");
        return false;
    }
    m_context = context;
    m_lastLocalSequence = 1;
    m_state = State::Active;
    error.clear();
    return true;
}

bool SessionTerminationSender::noteLocalSequence(quint64 sequence,
                                                QString &error) {
    if (m_state != State::Active || sequence <= m_lastLocalSequence) {
        error = QStringLiteral("Local session sequence is not increasing.");
        return false;
    }
    m_lastLocalSequence = sequence;
    error.clear();
    return true;
}

bool SessionTerminationSender::end(
    easydesk::client::signaling::SessionEndReason reason) {
    if (m_state != State::Active || !m_signaling.isRegistered()) {
        emit failed(QStringLiteral("E_SESSION_END_STATE"),
                    QStringLiteral("Session termination is not available."));
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    easydesk::client::signaling::SessionEndFields fields;
    fields.requestId = m_context.requestId;
    fields.sessionId = m_context.sessionId;
    fields.authorizationPayload = m_context.signedAcceptPayload;
    fields.fromDeviceId = m_context.localDeviceId;
    fields.toDeviceId = m_context.peerDeviceId;
    fields.sessionEpoch = m_context.sessionEpoch;
    fields.reason = reason;
    fields.sequence = m_lastLocalSequence + 1;
    fields.expiresAt = qMin(now + kEndLifetimeSeconds, m_context.expiresAt);
    if (fields.expiresAt <= now) {
        fail(QStringLiteral("E_SESSION_END_EXPIRED"),
             QStringLiteral("Session termination authorization expired."));
        return false;
    }

    QString error;
    const QByteArray signingBytes =
        easydesk::client::signaling::sessionEndSigningBytes(fields, error);
    const QByteArray signature = m_identity.sign(signingBytes);
    if (signingBytes.isEmpty() || signature.isEmpty()) {
        fail(QStringLiteral("E_SESSION_END_SIGNATURE"),
             error.isEmpty() ? m_identity.errorString() : error);
        return false;
    }
    const QByteArray envelope =
        easydesk::client::signaling::encodeSessionEndEnvelope(
            fields, signature, error);
    if (envelope.isEmpty() || !m_signaling.sendRegisteredMessage(envelope)) {
        fail(QStringLiteral("E_SESSION_END_SEND"),
             error.isEmpty()
                 ? QStringLiteral("Session termination could not be sent.")
                 : error);
        return false;
    }

    m_reason = reason;
    m_lastLocalSequence = fields.sequence;
    m_state = State::AwaitingDelivery;
    m_timeout.start(kDeliveryTimeoutMilliseconds);
    emit sent(reason, fields.sequence);
    return true;
}

void SessionTerminationSender::stop() {
    m_timeout.stop();
    m_context = {};
    m_state = State::Idle;
    m_reason = easydesk::client::signaling::SessionEndReason::UserStopped;
    m_lastLocalSequence = 0;
}

bool SessionTerminationSender::isActive() const {
    return m_state != State::Idle;
}

bool SessionTerminationSender::isAwaitingDelivery() const {
    return m_state == State::AwaitingDelivery;
}

void SessionTerminationSender::handleDelivery(const QByteArray &message) {
    if (m_state != State::AwaitingDelivery)
        return;

    easydesk::client::signaling::SessionDeliveryMessage delivery;
    QString error;
    if (!easydesk::client::signaling::decodeSessionDelivery(
            message, delivery, error)) {
        fail(QStringLiteral("E_SESSION_END_DELIVERY"), error);
        return;
    }
    if (delivery.messageType != QStringLiteral("session.end"))
        return;
    if (delivery.requestId != m_context.requestId ||
        delivery.sessionId != m_context.sessionId ||
        !delivery.hasSessionEpoch ||
        delivery.sessionEpoch != m_context.sessionEpoch ||
        delivery.sequence != m_lastLocalSequence) {
        fail(QStringLiteral("E_SESSION_END_DELIVERY"),
             QStringLiteral("Session termination receipt is invalid."));
        return;
    }

    const auto reason = m_reason;
    const quint64 sequence = m_lastLocalSequence;
    stop();
    emit delivered(reason, sequence);
}

void SessionTerminationSender::handleTimeout() {
    fail(QStringLiteral("E_SESSION_END_TIMEOUT"),
         QStringLiteral("Session termination delivery timed out."));
}

void SessionTerminationSender::fail(const QString &code,
                                    const QString &message) {
    stop();
    emit failed(code, message);
}
