#include "network/signaling/protocol/session/termination/session_end_decoder.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_reader.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumSignedPayloadBytes = 44 * 1024;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionMessageType = "session.end";
constexpr auto kSessionProtocolValue = "easydesk-session/2";

bool exactUnsigned(const QJsonValue &value,
                   quint64 minimum,
                   quint64 maximum,
                   quint64 &result) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(minimum) ||
        number > static_cast<double>(maximum)) {
        return false;
    }
    result = static_cast<quint64>(number);
    return true;
}

bool decodeCanonicalBase64(const QJsonValue &value,
                           int expectedSize,
                           int maximumSize,
                           QByteArray &decoded) {
    if (!value.isString())
        return false;
    const QByteArray encoded = value.toString().toLatin1();
    const auto bytes = decodeBase64Url(encoded);
    if (!bytes || bytes->isEmpty() || bytes->size() > maximumSize ||
        (expectedSize >= 0 && bytes->size() != expectedSize) ||
        encodeBase64Url(*bytes) != encoded) {
        return false;
    }
    decoded = *bytes;
    return true;
}

bool decodePayload(const QByteArray &payload,
                   const SessionEndContext &context,
                   DecodedSessionEnd &decoded,
                   QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto messageType = reader.readText(64);
    const auto authorizationDigest = reader.readBytes(32);
    const auto sessionEpoch = reader.readUnsigned();
    const auto reason = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 7 || !domain ||
        *domain != QString::fromLatin1(kSessionSigningDomain) || !messageType ||
        *messageType != QString::fromLatin1(kSessionMessageType) ||
        !authorizationDigest || authorizationDigest->size() != 32 ||
        *authorizationDigest != QCryptographicHash::hash(
                                    context.authorizationPayload,
                                    QCryptographicHash::Sha256) ||
        !sessionEpoch || *sessionEpoch != context.sessionEpoch || !reason ||
        *reason < static_cast<quint8>(SessionEndReason::UserStopped) ||
        *reason > static_cast<quint8>(SessionEndReason::ProtocolFailure) ||
        !sequence || *sequence < 2 ||
        *sequence > static_cast<quint64>(kMaximumExactJsonInteger) ||
        !expiresAt || *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) ||
        !reader.atEnd()) {
        error = QStringLiteral("Session end signed payload is invalid.");
        return false;
    }
    decoded.reason = static_cast<SessionEndReason>(*reason);
    decoded.sequence = *sequence;
    decoded.expiresAt = static_cast<qint64>(*expiresAt);
    return true;
}

}  // namespace

bool decodeSessionEndEnvelope(const QByteArray &message,
                              const SessionEndContext &context,
                              DecodedSessionEnd &decoded,
                              QString &error) {
    decoded = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session end is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    if (object.size() != 11 ||
        object.value(QStringLiteral("protocol")).toString() !=
            QString::fromLatin1(kSessionProtocolValue) ||
        object.value(QStringLiteral("type")).toString() !=
            QString::fromLatin1(kSessionMessageType) ||
        object.value(QStringLiteral("from_device_id")).toString() !=
            context.fromDeviceId ||
        object.value(QStringLiteral("to_device_id")).toString() !=
            context.toDeviceId) {
        error = QStringLiteral("Session end envelope type or route is invalid.");
        return false;
    }

    QByteArray requestId;
    QByteArray sessionId;
    QByteArray signedPayload;
    QByteArray signature;
    quint64 sessionEpoch = 0;
    quint64 sequence = 0;
    quint64 expiresAt = 0;
    if (!decodeCanonicalBase64(object.value(QStringLiteral("request_id")),
                               16, 16, requestId) ||
        requestId != context.requestId ||
        !decodeCanonicalBase64(object.value(QStringLiteral("session_id")),
                               16, 16, sessionId) ||
        sessionId != context.sessionId ||
        !exactUnsigned(object.value(QStringLiteral("session_epoch")), 1,
                       0xffffffffULL, sessionEpoch) ||
        sessionEpoch != context.sessionEpoch ||
        !exactUnsigned(object.value(QStringLiteral("seq")), 2,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       sequence) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       expiresAt) ||
        !decodeCanonicalBase64(object.value(QStringLiteral("signed_payload")),
                               -1, kMaximumSignedPayloadBytes,
                               signedPayload) ||
        !decodeCanonicalBase64(object.value(QStringLiteral("signature")),
                               64, 64, signature) ||
        !decodePayload(signedPayload, context, decoded, error) ||
        decoded.sequence != sequence ||
        decoded.expiresAt != static_cast<qint64>(expiresAt)) {
        if (error.isEmpty())
            error = QStringLiteral("Session end envelope fields are invalid.");
        return false;
    }

    SessionEndFields canonicalFields;
    canonicalFields.requestId = context.requestId;
    canonicalFields.sessionId = context.sessionId;
    canonicalFields.authorizationPayload = context.authorizationPayload;
    canonicalFields.fromDeviceId = context.fromDeviceId;
    canonicalFields.toDeviceId = context.toDeviceId;
    canonicalFields.sessionEpoch = context.sessionEpoch;
    canonicalFields.reason = decoded.reason;
    canonicalFields.sequence = decoded.sequence;
    canonicalFields.expiresAt = decoded.expiresAt;
    QString canonicalError;
    if (sessionEndSigningBytes(canonicalFields, canonicalError) != signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session end signed payload is not canonical.")
                    : canonicalError;
        return false;
    }

    decoded.signedPayload = signedPayload;
    decoded.signature = signature;
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
