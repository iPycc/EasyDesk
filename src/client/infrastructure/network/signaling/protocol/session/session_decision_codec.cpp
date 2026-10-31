#include "network/signaling/protocol/session/session_decision_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionProtocolValue = "easydesk-session/2";
constexpr auto kRejectMessageType = "session.reject";
constexpr auto kCancelMessageType = "session.cancel";

bool isKnownReason(SessionRejectReason reason) {
    return reason == SessionRejectReason::Declined ||
           reason == SessionRejectReason::Unavailable;
}

bool isKnownReason(SessionCancelReason reason) {
    return reason == SessionCancelReason::UserCancelled ||
           reason == SessionCancelReason::NegotiationFailed;
}

template <typename Reason>
QByteArray encodeSigningBytes(const SessionDecisionFields &fields,
                              Reason reason,
                              const char *messageType,
                              QString &error) {
    const QByteArray expectedRequestPayload =
        sessionRequestSigningBytes(fields.request, error);
    if (expectedRequestPayload.isEmpty() ||
        expectedRequestPayload != fields.signedRequestPayload) {
        if (error.isEmpty())
            error = QStringLiteral("Session decision request payload is not canonical.");
        return {};
    }
    if (!isKnownReason(reason) || fields.sequence == 0 ||
        fields.sequence > static_cast<quint64>(kMaximumExactJsonInteger) ||
        fields.expiresAt <= 0 ||
        fields.expiresAt > kMaximumExactJsonInteger) {
        error = QStringLiteral("Session decision fields are invalid.");
        return {};
    }

    const QByteArray requestDigest = QCryptographicHash::hash(
        fields.signedRequestPayload, QCryptographicHash::Sha256);
    cbor::DeterministicCborWriter writer;
    writer.beginArray(6);
    writer.writeText(QString::fromLatin1(kSessionSigningDomain));
    writer.writeText(QString::fromLatin1(messageType));
    writer.writeBytes(requestDigest);
    writer.writeUnsigned(static_cast<quint8>(reason));
    writer.writeUnsigned(fields.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.expiresAt));
    error.clear();
    return writer.takeBytes();
}

template <typename Reason>
QByteArray encodeEnvelope(const SessionDecisionFields &fields,
                          Reason reason,
                          const QByteArray &signature,
                          const char *messageType,
                          bool reject,
                          QString &error) {
    const QByteArray signedPayload =
        encodeSigningBytes(fields, reason, messageType, error);
    if (signedPayload.isEmpty())
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session decision signature must contain exactly 64 bytes.");
        return {};
    }

    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kSessionProtocolValue));
    object.insert(QStringLiteral("type"), QString::fromLatin1(messageType));
    object.insert(QStringLiteral("request_id"),
                  QString::fromLatin1(encodeBase64Url(fields.request.requestId)));
    object.insert(QStringLiteral("session_id"),
                  QString::fromLatin1(encodeBase64Url(fields.request.sessionId)));
    object.insert(QStringLiteral("seq"), static_cast<double>(fields.sequence));
    object.insert(QStringLiteral("expires_at"), static_cast<double>(fields.expiresAt));
    object.insert(QStringLiteral("from_device_id"),
                  reject ? fields.request.toDeviceId : fields.request.fromDeviceId);
    object.insert(QStringLiteral("to_device_id"),
                  reject ? fields.request.fromDeviceId : fields.request.toDeviceId);
    object.insert(QStringLiteral("signed_payload"),
                  QString::fromLatin1(encodeBase64Url(signedPayload)));
    object.insert(QStringLiteral("signature"),
                  QString::fromLatin1(encodeBase64Url(signature)));
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

QByteArray sessionRejectSigningBytes(const SessionDecisionFields &fields,
                                     SessionRejectReason reason,
                                     QString &error) {
    return encodeSigningBytes(fields, reason, kRejectMessageType, error);
}

QByteArray encodeSessionRejectEnvelope(const SessionDecisionFields &fields,
                                       SessionRejectReason reason,
                                       const QByteArray &signature,
                                       QString &error) {
    return encodeEnvelope(fields, reason, signature, kRejectMessageType, true, error);
}

QByteArray sessionCancelSigningBytes(const SessionDecisionFields &fields,
                                     SessionCancelReason reason,
                                     QString &error) {
    return encodeSigningBytes(fields, reason, kCancelMessageType, error);
}

QByteArray encodeSessionCancelEnvelope(const SessionDecisionFields &fields,
                                       SessionCancelReason reason,
                                       const QByteArray &signature,
                                       QString &error) {
    return encodeEnvelope(fields, reason, signature, kCancelMessageType, false, error);
}

}  // namespace easydesk::client::signaling
