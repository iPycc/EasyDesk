#include "network/signaling/protocol/session/session_accept_decoder.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_reader.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
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
constexpr auto kSessionMessageType = "session.accept";
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

bool decodeIdentifier(const QJsonValue &value, QByteArray &identifier) {
    if (!value.isString())
        return false;
    const auto decoded = decodeBase64Url(value.toString().toLatin1());
    if (!decoded || decoded->size() != 16)
        return false;
    identifier = *decoded;
    return true;
}

bool decodePayload(const QByteArray &payload,
                   const SessionRequestFields &request,
                   const QByteArray &signedRequestPayload,
                   SessionAcceptFields &fields,
                   QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto messageType = reader.readText(64);
    const auto requestDigest = reader.readBytes(32);
    const auto nonce = reader.readBytes(32);
    const auto grantCount = reader.readArraySize();
    if (!arraySize || *arraySize != 8 || !domain ||
        *domain != QString::fromLatin1(kSessionSigningDomain) || !messageType ||
        *messageType != QString::fromLatin1(kSessionMessageType) || !requestDigest ||
        requestDigest->size() != 32 ||
        *requestDigest != QCryptographicHash::hash(signedRequestPayload,
                                                   QCryptographicHash::Sha256) ||
        !nonce || nonce->size() != 32 || !grantCount || *grantCount == 0 ||
        *grantCount > 4) {
        error = QStringLiteral("Session acceptance signed payload header is invalid.");
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
            error = QStringLiteral("Session acceptance capability payload is invalid.");
            return false;
        }
        grants.append({*subject, static_cast<SessionCapability>(*capability)});
    }

    const auto sessionEpoch = reader.readUnsigned();
    const auto permissionEpoch = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!sessionEpoch || *sessionEpoch == 0 || *sessionEpoch > 0xffffffffULL ||
        !permissionEpoch || *permissionEpoch == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) || !reader.atEnd()) {
        error = QStringLiteral("Session acceptance authorization payload is invalid.");
        return false;
    }

    fields.request = request;
    fields.signedRequestPayload = signedRequestPayload;
    fields.nonce = *nonce;
    fields.grantedGrants = std::move(grants);
    fields.sessionEpoch = static_cast<quint32>(*sessionEpoch);
    fields.permissionEpoch = *permissionEpoch;
    fields.expiresAt = static_cast<qint64>(*expiresAt);
    return true;
}

}  // namespace

bool decodeSessionAcceptEnvelope(const QByteArray &message,
                                 const SessionRequestFields &request,
                                 const QByteArray &signedRequestPayload,
                                 DecodedSessionAccept &accept,
                                 QString &error) {
    accept = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session acceptance is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue protocol = object.value(QStringLiteral("protocol"));
    const QJsonValue fromDeviceId = object.value(QStringLiteral("from_device_id"));
    const QJsonValue toDeviceId = object.value(QStringLiteral("to_device_id"));
    QByteArray envelopeRequestId;
    QByteArray envelopeSessionId;
    quint64 sequence = 0;
    quint64 envelopeEpoch = 0;
    quint64 envelopeExpiry = 0;
    if (object.size() != 11 || !type.isString() ||
        type.toString() != QString::fromLatin1(kSessionMessageType) ||
        !protocol.isString() ||
        protocol.toString() != QString::fromLatin1(kSessionProtocolValue) ||
        !fromDeviceId.isString() || fromDeviceId.toString() != request.toDeviceId ||
        !toDeviceId.isString() || toDeviceId.toString() != request.fromDeviceId ||
        !decodeIdentifier(object.value(QStringLiteral("request_id")), envelopeRequestId) ||
        envelopeRequestId != request.requestId ||
        !decodeIdentifier(object.value(QStringLiteral("session_id")), envelopeSessionId) ||
        envelopeSessionId != request.sessionId ||
        !exactUnsigned(object.value(QStringLiteral("session_epoch")), 1,
                       0xffffffffULL, envelopeEpoch) ||
        !exactUnsigned(object.value(QStringLiteral("seq")), 1, 1, sequence) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger), envelopeExpiry)) {
        error = QStringLiteral("Session acceptance envelope fields are invalid.");
        return false;
    }

    if (!object.value(QStringLiteral("signed_payload")).isString() ||
        !object.value(QStringLiteral("signature")).isString()) {
        error = QStringLiteral("Session acceptance authentication fields are missing.");
        return false;
    }
    const auto signedPayload = decodeBase64Url(
        object.value(QStringLiteral("signed_payload")).toString().toLatin1());
    const auto signature = decodeBase64Url(
        object.value(QStringLiteral("signature")).toString().toLatin1());
    if (!signedPayload || signedPayload->isEmpty() ||
        signedPayload->size() > kMaximumSignedPayloadBytes || !signature ||
        signature->size() != 64) {
        error = QStringLiteral("Session acceptance authentication fields are invalid.");
        return false;
    }

    SessionAcceptFields fields;
    if (!decodePayload(*signedPayload, request, signedRequestPayload, fields, error) ||
        fields.sessionEpoch != static_cast<quint32>(envelopeEpoch) ||
        fields.expiresAt != static_cast<qint64>(envelopeExpiry)) {
        if (error.isEmpty())
            error = QStringLiteral("Session acceptance envelope does not match its payload.");
        return false;
    }

    QString canonicalError;
    if (sessionAcceptSigningBytes(fields, canonicalError) != *signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session acceptance signed payload is not canonical.")
                    : canonicalError;
        return false;
    }

    accept.fields = std::move(fields);
    accept.signedPayload = *signedPayload;
    accept.signature = *signature;
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
