#include "network/signaling/protocol/session/session_request_decoder.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_reader.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <utility>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumSignedPayloadBytes = 44 * 1024;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionMessageType = "session.request";
constexpr auto kSessionProtocolValue = "easydesk-session/2";

bool isDeviceId(const QString &value) {
    if (value.size() != 9)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

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

bool decodeCanonicalBytes(const QJsonValue &value,
                          int expectedBytes,
                          QByteArray &decoded) {
    if (!value.isString())
        return false;
    const auto bytes = decodeBase64Url(value.toString().toLatin1());
    if (!bytes || bytes->size() != expectedBytes)
        return false;
    decoded = *bytes;
    return true;
}

bool decodePayload(const QByteArray &payload,
                   SessionRequestFields &fields,
                   QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto messageType = reader.readText(64);
    const auto requestId = reader.readBytes(16);
    const auto sessionId = reader.readBytes(16);
    const auto requesterPublicKey = reader.readBytes(32);
    const auto targetPublicKey = reader.readBytes(32);
    const auto requesterRole = reader.readUnsigned();
    const auto nonce = reader.readBytes(32);
    const auto grantCount = reader.readArraySize();
    if (!arraySize || *arraySize != 10 || !domain ||
        *domain != QString::fromLatin1(kSessionSigningDomain) || !messageType ||
        *messageType != QString::fromLatin1(kSessionMessageType) || !requestId ||
        requestId->size() != 16 || !sessionId || sessionId->size() != 16 ||
        !requesterPublicKey || requesterPublicKey->size() != 32 || !targetPublicKey ||
        targetPublicKey->size() != 32 || !requesterRole ||
        (*requesterRole != static_cast<quint8>(SessionParticipantRole::Controller) &&
         *requesterRole != static_cast<quint8>(SessionParticipantRole::Controlled)) ||
        !nonce || nonce->size() != 32 || !grantCount || *grantCount == 0 ||
        *grantCount > 4) {
        error = QStringLiteral("Session request signed payload header is invalid.");
        return false;
    }

    QVector<SessionCapabilityGrant> grants;
    grants.reserve(static_cast<int>(*grantCount));
    for (quint64 index = 0; index < *grantCount; ++index) {
        const auto entrySize = reader.readArraySize();
        const auto subject = reader.readBytes(32);
        const auto capability = reader.readUnsigned();
        if (!entrySize || *entrySize != 2 || !subject || subject->size() != 32 ||
            !capability || *capability < static_cast<quint8>(SessionCapability::ViewScreen) ||
            *capability > static_cast<quint8>(SessionCapability::FileReceive)) {
            error = QStringLiteral("Session request capability payload is invalid.");
            return false;
        }
        grants.append({*subject, static_cast<SessionCapability>(*capability)});
    }

    const auto expiresAt = reader.readUnsigned();
    if (!expiresAt || *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) || !reader.atEnd()) {
        error = QStringLiteral("Session request signed payload expiry is invalid.");
        return false;
    }

    fields.requestId = *requestId;
    fields.sessionId = *sessionId;
    fields.requesterPublicKey = *requesterPublicKey;
    fields.targetPublicKey = *targetPublicKey;
    fields.requesterRole = static_cast<SessionParticipantRole>(*requesterRole);
    fields.nonce = *nonce;
    fields.requestedGrants = std::move(grants);
    fields.expiresAt = static_cast<qint64>(*expiresAt);
    return true;
}

}  // namespace

bool decodeSessionRequestEnvelope(const QByteArray &message,
                                  DecodedSessionRequest &request,
                                  QString &error) {
    request = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session request is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue protocol = object.value(QStringLiteral("protocol"));
    const QJsonValue fromDeviceId = object.value(QStringLiteral("from_device_id"));
    const QJsonValue toDeviceId = object.value(QStringLiteral("to_device_id"));
    quint64 sequence = 0;
    quint64 envelopeExpiry = 0;
    QByteArray envelopeRequestId;
    QByteArray envelopeSessionId;
    if (object.size() != 10 || !type.isString() ||
        type.toString() != QString::fromLatin1(kSessionMessageType) ||
        !protocol.isString() ||
        protocol.toString() != QString::fromLatin1(kSessionProtocolValue) ||
        !fromDeviceId.isString() || !toDeviceId.isString() ||
        !isDeviceId(fromDeviceId.toString()) || !isDeviceId(toDeviceId.toString()) ||
        fromDeviceId.toString() == toDeviceId.toString() ||
        !decodeCanonicalBytes(object.value(QStringLiteral("request_id")), 16,
                              envelopeRequestId) ||
        !decodeCanonicalBytes(object.value(QStringLiteral("session_id")), 16,
                              envelopeSessionId) ||
        !exactUnsigned(object.value(QStringLiteral("seq")), 1, 1, sequence) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger), envelopeExpiry)) {
        error = QStringLiteral("Session request envelope fields are invalid.");
        return false;
    }

    if (!object.value(QStringLiteral("signed_payload")).isString() ||
        !object.value(QStringLiteral("signature")).isString()) {
        error = QStringLiteral("Session request authentication fields are missing.");
        return false;
    }
    const auto signedPayload = decodeBase64Url(
        object.value(QStringLiteral("signed_payload")).toString().toLatin1());
    const auto signature = decodeBase64Url(
        object.value(QStringLiteral("signature")).toString().toLatin1());
    if (!signedPayload || signedPayload->isEmpty() ||
        signedPayload->size() > kMaximumSignedPayloadBytes || !signature ||
        signature->size() != 64) {
        error = QStringLiteral("Session request authentication fields are invalid.");
        return false;
    }

    SessionRequestFields fields;
    fields.fromDeviceId = fromDeviceId.toString();
    fields.toDeviceId = toDeviceId.toString();
    if (!decodePayload(*signedPayload, fields, error) ||
        fields.requestId != envelopeRequestId || fields.sessionId != envelopeSessionId ||
        fields.expiresAt != static_cast<qint64>(envelopeExpiry)) {
        if (error.isEmpty())
            error = QStringLiteral("Session request envelope does not match its signed payload.");
        return false;
    }

    QString canonicalError;
    if (sessionRequestSigningBytes(fields, canonicalError) != *signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session request signed payload is not canonical.")
                    : canonicalError;
        return false;
    }

    request.fields = std::move(fields);
    request.signedPayload = *signedPayload;
    request.signature = *signature;
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
