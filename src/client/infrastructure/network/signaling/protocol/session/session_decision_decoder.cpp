#include "network/signaling/protocol/session/session_decision_decoder.h"

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
constexpr auto kSessionProtocolValue = "easydesk-session/2";
constexpr auto kRejectMessageType = "session.reject";
constexpr auto kCancelMessageType = "session.cancel";

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

bool isKnownReason(SessionDecisionKind kind, quint64 reason) {
    if (kind == SessionDecisionKind::Reject) {
        return reason == static_cast<quint8>(SessionRejectReason::Declined) ||
               reason == static_cast<quint8>(SessionRejectReason::Unavailable);
    }
    return reason == static_cast<quint8>(SessionCancelReason::UserCancelled) ||
           reason == static_cast<quint8>(SessionCancelReason::NegotiationFailed);
}

bool decodePayload(const QByteArray &payload,
                   const QByteArray &signedRequestPayload,
                   SessionDecisionKind kind,
                   DecodedSessionDecision &decision,
                   QString &error) {
    const QString expectedType = kind == SessionDecisionKind::Reject
                                     ? QString::fromLatin1(kRejectMessageType)
                                     : QString::fromLatin1(kCancelMessageType);
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto messageType = reader.readText(64);
    const auto requestDigest = reader.readBytes(32);
    const auto reason = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 6 || !domain ||
        *domain != QString::fromLatin1(kSessionSigningDomain) || !messageType ||
        *messageType != expectedType || !requestDigest || requestDigest->size() != 32 ||
        *requestDigest != QCryptographicHash::hash(signedRequestPayload,
                                                   QCryptographicHash::Sha256) ||
        !reason || !isKnownReason(kind, *reason) || !sequence || *sequence == 0 ||
        *sequence > static_cast<quint64>(kMaximumExactJsonInteger) || !expiresAt ||
        *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) || !reader.atEnd()) {
        error = QStringLiteral("Session decision signed payload is invalid.");
        return false;
    }
    decision.kind = kind;
    decision.reason = static_cast<quint8>(*reason);
    decision.sequence = *sequence;
    decision.expiresAt = static_cast<qint64>(*expiresAt);
    return true;
}

}  // namespace

bool decodeSessionDecisionEnvelope(const QByteArray &message,
                                   const SessionRequestFields &request,
                                   const QByteArray &signedRequestPayload,
                                   DecodedSessionDecision &decision,
                                   QString &error) {
    decision = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session decision is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue protocol = object.value(QStringLiteral("protocol"));
    if (!type.isString()) {
        error = QStringLiteral("Session decision type is missing.");
        return false;
    }
    const SessionDecisionKind kind = type.toString() == QString::fromLatin1(kRejectMessageType)
                                         ? SessionDecisionKind::Reject
                                         : type.toString() == QString::fromLatin1(kCancelMessageType)
                                               ? SessionDecisionKind::Cancel
                                               : SessionDecisionKind::Reject;
    if ((type.toString() != QString::fromLatin1(kRejectMessageType) &&
         type.toString() != QString::fromLatin1(kCancelMessageType)) ||
        object.size() != 10 || !protocol.isString() ||
        protocol.toString() != QString::fromLatin1(kSessionProtocolValue)) {
        error = QStringLiteral("Session decision envelope type or protocol is invalid.");
        return false;
    }

    const QString expectedFrom = kind == SessionDecisionKind::Reject
                                     ? request.toDeviceId
                                     : request.fromDeviceId;
    const QString expectedTo = kind == SessionDecisionKind::Reject
                                   ? request.fromDeviceId
                                   : request.toDeviceId;
    QByteArray envelopeRequestId;
    QByteArray envelopeSessionId;
    quint64 envelopeSequence = 0;
    quint64 envelopeExpiry = 0;
    if (!object.value(QStringLiteral("from_device_id")).isString() ||
        object.value(QStringLiteral("from_device_id")).toString() != expectedFrom ||
        !object.value(QStringLiteral("to_device_id")).isString() ||
        object.value(QStringLiteral("to_device_id")).toString() != expectedTo ||
        !decodeIdentifier(object.value(QStringLiteral("request_id")), envelopeRequestId) ||
        envelopeRequestId != request.requestId ||
        !decodeIdentifier(object.value(QStringLiteral("session_id")), envelopeSessionId) ||
        envelopeSessionId != request.sessionId ||
        !exactUnsigned(object.value(QStringLiteral("seq")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       envelopeSequence) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger), envelopeExpiry)) {
        error = QStringLiteral("Session decision route or sequence is invalid.");
        return false;
    }

    if (!object.value(QStringLiteral("signed_payload")).isString() ||
        !object.value(QStringLiteral("signature")).isString()) {
        error = QStringLiteral("Session decision authentication fields are missing.");
        return false;
    }
    const auto signedPayload = decodeBase64Url(
        object.value(QStringLiteral("signed_payload")).toString().toLatin1());
    const auto signature = decodeBase64Url(
        object.value(QStringLiteral("signature")).toString().toLatin1());
    if (!signedPayload || signedPayload->isEmpty() ||
        signedPayload->size() > kMaximumSignedPayloadBytes || !signature ||
        signature->size() != 64 ||
        !decodePayload(*signedPayload, signedRequestPayload, kind, decision, error) ||
        decision.sequence != envelopeSequence ||
        decision.expiresAt != static_cast<qint64>(envelopeExpiry)) {
        if (error.isEmpty())
            error = QStringLiteral("Session decision authentication payload is invalid.");
        return false;
    }

    SessionDecisionFields fields;
    fields.request = request;
    fields.signedRequestPayload = signedRequestPayload;
    fields.sequence = decision.sequence;
    fields.expiresAt = decision.expiresAt;
    QString canonicalError;
    const QByteArray canonical = kind == SessionDecisionKind::Reject
        ? sessionRejectSigningBytes(fields,
                                    static_cast<SessionRejectReason>(decision.reason),
                                    canonicalError)
        : sessionCancelSigningBytes(fields,
                                    static_cast<SessionCancelReason>(decision.reason),
                                    canonicalError);
    if (canonical != *signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session decision signed payload is not canonical.")
                    : canonicalError;
        return false;
    }

    decision.signedPayload = *signedPayload;
    decision.signature = *signature;
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
