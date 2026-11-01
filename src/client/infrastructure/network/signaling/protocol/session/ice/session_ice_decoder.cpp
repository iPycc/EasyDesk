#include "network/signaling/protocol/session/ice/session_ice_decoder.h"

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
constexpr int kMaximumCandidateBatchSize = 8;
constexpr int kMaximumMidBytes = 64;
constexpr int kMaximumUfragBytes = 256;
constexpr int kMaximumCandidateBytes = 2048;
constexpr auto kSigningDomain = "EasyDesk-Session-v2";
constexpr auto kProtocol = "easydesk-session/2";
constexpr auto kCandidatesType = "session.candidates";
constexpr auto kGatheringCompleteType = "session.gathering_complete";

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

bool decodeIdentifier(const QJsonValue &value, const QByteArray &expected) {
    if (!value.isString())
        return false;
    const auto decoded = decodeBase64Url(value.toString().toLatin1());
    return decoded && decoded->size() == 16 && *decoded == expected;
}

bool containsDigits(const QString &value, int length) {
    if (value.size() != length)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool contextIsValid(const SessionIceContext &context) {
    return context.requestId.size() == 16 && context.sessionId.size() == 16 &&
           context.sessionEpoch > 0 && context.descriptionSequence >= 2 &&
           context.descriptionSequence <
               static_cast<quint64>(kMaximumExactJsonInteger) &&
           containsDigits(context.fromDeviceId, 9) &&
           containsDigits(context.toDeviceId, 9) &&
           context.fromDeviceId != context.toDeviceId &&
           !context.signedDescriptionPayload.isEmpty() &&
           context.signedDescriptionPayload.size() <= kMaximumSignedPayloadBytes;
}

bool decodeEnvelope(const QByteArray &message,
                    const SessionIceContext &context,
                    const char *expectedType,
                    SessionDescriptionRoute &route,
                    QByteArray &signedPayload,
                    QByteArray &signature,
                    QString &error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session ICE signal is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    quint64 sessionEpoch = 0;
    quint64 iceGeneration = 0;
    quint64 sequence = 0;
    quint64 expiresAt = 0;
    if (object.size() != 12 ||
        !object.value(QStringLiteral("type")).isString() ||
        object.value(QStringLiteral("type")).toString() !=
            QString::fromLatin1(expectedType) ||
        !object.value(QStringLiteral("protocol")).isString() ||
        object.value(QStringLiteral("protocol")).toString() !=
            QString::fromLatin1(kProtocol) ||
        !decodeIdentifier(object.value(QStringLiteral("request_id")),
                          context.requestId) ||
        !decodeIdentifier(object.value(QStringLiteral("session_id")),
                          context.sessionId) ||
        !exactUnsigned(object.value(QStringLiteral("session_epoch")),
                       1, 0xffffffffULL, sessionEpoch) ||
        sessionEpoch != context.sessionEpoch ||
        !exactUnsigned(object.value(QStringLiteral("ice_generation")),
                       0, 0xffffffffULL, iceGeneration) ||
        iceGeneration != context.iceGeneration ||
        !exactUnsigned(object.value(QStringLiteral("seq")),
                       3,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       sequence) ||
        sequence <= context.descriptionSequence ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")),
                       1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       expiresAt) ||
        !object.value(QStringLiteral("from_device_id")).isString() ||
        object.value(QStringLiteral("from_device_id")).toString() !=
            context.fromDeviceId ||
        !object.value(QStringLiteral("to_device_id")).isString() ||
        object.value(QStringLiteral("to_device_id")).toString() !=
            context.toDeviceId ||
        !object.value(QStringLiteral("signed_payload")).isString() ||
        !object.value(QStringLiteral("signature")).isString()) {
        error = QStringLiteral("Session ICE envelope fields are invalid.");
        return false;
    }

    const auto decodedPayload = decodeBase64Url(
        object.value(QStringLiteral("signed_payload")).toString().toLatin1());
    const auto decodedSignature = decodeBase64Url(
        object.value(QStringLiteral("signature")).toString().toLatin1());
    if (!decodedPayload || decodedPayload->isEmpty() ||
        decodedPayload->size() > kMaximumSignedPayloadBytes ||
        !decodedSignature || decodedSignature->size() != 64) {
        error = QStringLiteral("Session ICE authentication fields are invalid.");
        return false;
    }

    route.requestId = context.requestId;
    route.sessionId = context.sessionId;
    route.sessionEpoch = static_cast<quint32>(sessionEpoch);
    route.iceGeneration = static_cast<quint32>(iceGeneration);
    route.sequence = sequence;
    route.expiresAt = static_cast<qint64>(expiresAt);
    route.fromDeviceId = context.fromDeviceId;
    route.toDeviceId = context.toDeviceId;
    signedPayload = *decodedPayload;
    signature = *decodedSignature;
    return true;
}

bool decodeCandidatesPayload(const QByteArray &payload,
                             const SessionIceContext &context,
                             SessionCandidatesFields &fields,
                             QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto type = reader.readText(64);
    const auto descriptionDigest = reader.readBytes(32);
    const auto iceGeneration = reader.readUnsigned();
    const auto candidateCount = reader.readArraySize();
    if (!arraySize || *arraySize != 7 || !domain ||
        *domain != QString::fromLatin1(kSigningDomain) || !type ||
        *type != QString::fromLatin1(kCandidatesType) || !descriptionDigest ||
        *descriptionDigest !=
            QCryptographicHash::hash(context.signedDescriptionPayload,
                                     QCryptographicHash::Sha256) ||
        !iceGeneration || *iceGeneration != context.iceGeneration ||
        !candidateCount || *candidateCount == 0 ||
        *candidateCount > kMaximumCandidateBatchSize) {
        error = QStringLiteral("Session ICE candidate payload is invalid.");
        return false;
    }

    QVector<SessionIceCandidate> candidates;
    candidates.reserve(static_cast<int>(*candidateCount));
    for (quint64 index = 0; index < *candidateCount; ++index) {
        const auto entrySize = reader.readArraySize();
        const auto mid = reader.readText(kMaximumMidBytes);
        const auto mline = reader.readUnsigned();
        const auto ufrag = reader.readText(kMaximumUfragBytes);
        const auto candidate = reader.readText(kMaximumCandidateBytes);
        if (!entrySize || *entrySize != 4 || !mid || !mline ||
            *mline > 255 || !ufrag || !candidate) {
            error = QStringLiteral("Session ICE candidate payload is invalid.");
            return false;
        }
        candidates.append({*mid,
                           static_cast<quint32>(*mline),
                           *ufrag,
                           *candidate});
    }

    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!sequence || *sequence != fields.route.sequence || !expiresAt ||
        *expiresAt != static_cast<quint64>(fields.route.expiresAt) ||
        !reader.atEnd()) {
        error = QStringLiteral("Session ICE candidate payload is invalid.");
        return false;
    }
    fields.signedDescriptionPayload = context.signedDescriptionPayload;
    fields.candidates = std::move(candidates);
    return true;
}

bool decodeGatheringCompletePayload(const QByteArray &payload,
                                    const SessionIceContext &context,
                                    SessionGatheringCompleteFields &fields,
                                    QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto type = reader.readText(64);
    const auto descriptionDigest = reader.readBytes(32);
    const auto iceGeneration = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 6 || !domain ||
        *domain != QString::fromLatin1(kSigningDomain) || !type ||
        *type != QString::fromLatin1(kGatheringCompleteType) ||
        !descriptionDigest ||
        *descriptionDigest !=
            QCryptographicHash::hash(context.signedDescriptionPayload,
                                     QCryptographicHash::Sha256) ||
        !iceGeneration || *iceGeneration != context.iceGeneration ||
        !sequence || *sequence != fields.route.sequence || !expiresAt ||
        *expiresAt != static_cast<quint64>(fields.route.expiresAt) ||
        !reader.atEnd()) {
        error = QStringLiteral("Session ICE gathering payload is invalid.");
        return false;
    }
    fields.signedDescriptionPayload = context.signedDescriptionPayload;
    return true;
}

}  // namespace

bool decodeSessionCandidatesEnvelope(const QByteArray &message,
                                     const SessionIceContext &context,
                                     DecodedSessionCandidates &candidates,
                                     QString &error) {
    candidates = {};
    if (!contextIsValid(context)) {
        error = QStringLiteral("Session ICE context is invalid.");
        return false;
    }
    if (!decodeEnvelope(message,
                        context,
                        kCandidatesType,
                        candidates.fields.route,
                        candidates.signedPayload,
                        candidates.signature,
                        error) ||
        !decodeCandidatesPayload(candidates.signedPayload,
                                 context,
                                 candidates.fields,
                                 error)) {
        return false;
    }
    QString canonicalError;
    if (sessionCandidatesSigningBytes(candidates.fields, canonicalError) !=
        candidates.signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session ICE candidate payload is not canonical.")
                    : canonicalError;
        return false;
    }
    error.clear();
    return true;
}

bool decodeSessionGatheringCompleteEnvelope(
    const QByteArray &message,
    const SessionIceContext &context,
    DecodedSessionGatheringComplete &complete,
    QString &error) {
    complete = {};
    if (!contextIsValid(context)) {
        error = QStringLiteral("Session ICE context is invalid.");
        return false;
    }
    if (!decodeEnvelope(message,
                        context,
                        kGatheringCompleteType,
                        complete.fields.route,
                        complete.signedPayload,
                        complete.signature,
                        error) ||
        !decodeGatheringCompletePayload(complete.signedPayload,
                                        context,
                                        complete.fields,
                                        error)) {
        return false;
    }
    QString canonicalError;
    if (sessionGatheringCompleteSigningBytes(complete.fields, canonicalError) !=
        complete.signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session ICE gathering payload is not canonical.")
                    : canonicalError;
        return false;
    }
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
