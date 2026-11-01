#include "network/signaling/protocol/session/termination/session_end_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumAuthorizationPayloadBytes = 44 * 1024;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionMessageType = "session.end";
constexpr auto kSessionProtocolValue = "easydesk-session/2";

bool validDeviceId(const QString &value) {
    if (value.size() != 9)
        return false;
    for (const QChar character : value) {
        if (!character.isDigit())
            return false;
    }
    return true;
}

bool validReason(SessionEndReason reason) {
    return reason == SessionEndReason::UserStopped ||
           reason == SessionEndReason::ConnectionFailed ||
           reason == SessionEndReason::AuthorizationExpired ||
           reason == SessionEndReason::ProtocolFailure;
}

bool validate(const SessionEndFields &fields, QString &error) {
    if (fields.requestId.size() != 16 || fields.sessionId.size() != 16) {
        error = QStringLiteral("Session end identifiers must contain exactly 16 bytes.");
        return false;
    }
    if (fields.authorizationPayload.isEmpty() ||
        fields.authorizationPayload.size() > kMaximumAuthorizationPayloadBytes) {
        error = QStringLiteral("Session end authorization payload is invalid.");
        return false;
    }
    if (!validDeviceId(fields.fromDeviceId) ||
        !validDeviceId(fields.toDeviceId) ||
        fields.fromDeviceId == fields.toDeviceId) {
        error = QStringLiteral("Session end device route is invalid.");
        return false;
    }
    if (fields.sessionEpoch == 0 || !validReason(fields.reason) ||
        fields.sequence < 2 ||
        fields.sequence > static_cast<quint64>(kMaximumExactJsonInteger) ||
        fields.expiresAt <= 0 || fields.expiresAt > kMaximumExactJsonInteger) {
        error = QStringLiteral("Session end epoch, reason, sequence, or expiry is invalid.");
        return false;
    }
    error.clear();
    return true;
}

QByteArray encodePayload(const SessionEndFields &fields) {
    cbor::DeterministicCborWriter writer;
    writer.beginArray(7);
    writer.writeText(QString::fromLatin1(kSessionSigningDomain));
    writer.writeText(QString::fromLatin1(kSessionMessageType));
    writer.writeBytes(QCryptographicHash::hash(fields.authorizationPayload,
                                               QCryptographicHash::Sha256));
    writer.writeUnsigned(fields.sessionEpoch);
    writer.writeUnsigned(static_cast<quint8>(fields.reason));
    writer.writeUnsigned(fields.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.expiresAt));
    return writer.takeBytes();
}

}  // namespace

QByteArray sessionEndSigningBytes(const SessionEndFields &fields,
                                  QString &error) {
    if (!validate(fields, error))
        return {};
    return encodePayload(fields);
}

QByteArray encodeSessionEndEnvelope(const SessionEndFields &fields,
                                    const QByteArray &signature,
                                    QString &error) {
    if (!validate(fields, error))
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session end signature must contain exactly 64 bytes.");
        return {};
    }

    const QByteArray signedPayload = encodePayload(fields);
    QJsonObject object;
    object.insert(QStringLiteral("protocol"),
                  QString::fromLatin1(kSessionProtocolValue));
    object.insert(QStringLiteral("type"), QString::fromLatin1(kSessionMessageType));
    object.insert(QStringLiteral("request_id"),
                  QString::fromLatin1(encodeBase64Url(fields.requestId)));
    object.insert(QStringLiteral("session_id"),
                  QString::fromLatin1(encodeBase64Url(fields.sessionId)));
    object.insert(QStringLiteral("session_epoch"),
                  static_cast<double>(fields.sessionEpoch));
    object.insert(QStringLiteral("seq"), static_cast<double>(fields.sequence));
    object.insert(QStringLiteral("expires_at"),
                  static_cast<double>(fields.expiresAt));
    object.insert(QStringLiteral("from_device_id"), fields.fromDeviceId);
    object.insert(QStringLiteral("to_device_id"), fields.toDeviceId);
    object.insert(QStringLiteral("signed_payload"),
                  QString::fromLatin1(encodeBase64Url(signedPayload)));
    object.insert(QStringLiteral("signature"),
                  QString::fromLatin1(encodeBase64Url(signature)));
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace easydesk::client::signaling
