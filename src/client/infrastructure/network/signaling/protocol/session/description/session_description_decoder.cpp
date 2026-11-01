#include "network/signaling/protocol/session/description/session_description_decoder.h"

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
constexpr int kMaximumSessionDescriptionBytes = 32 * 1024;
constexpr int kMaximumSignedPayloadBytes = 44 * 1024;
constexpr auto kSigningDomain = "EasyDesk-Session-v2";
constexpr auto kProtocol = "easydesk-session/2";
constexpr auto kOfferType = "session.offer";
constexpr auto kAnswerType = "session.answer";

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

bool decodeIdentifier(const QJsonValue &value,
                      const QByteArray &expected) {
    if (!value.isString())
        return false;
    const auto decoded = decodeBase64Url(value.toString().toLatin1());
    return decoded && decoded->size() == 16 && *decoded == expected;
}

bool contextIsValid(const SessionDescriptionContext &context) {
    return context.requestId.size() == 16 && context.sessionId.size() == 16 &&
           context.sessionEpoch > 0 && !context.offererDeviceId.isEmpty() &&
           !context.answererDeviceId.isEmpty() &&
           context.offererDeviceId != context.answererDeviceId &&
           !context.signedAcceptPayload.isEmpty();
}

bool decodeEnvelope(const QByteArray &message,
                    const SessionDescriptionContext &context,
                    const char *expectedType,
                    const QString &expectedFrom,
                    const QString &expectedTo,
                    SessionDescriptionRoute &route,
                    QByteArray &signedPayload,
                    QByteArray &signature,
                    QString &error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session description is not a valid JSON object.");
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
        !exactUnsigned(object.value(QStringLiteral("seq")),
                       2,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       sequence) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")),
                       1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       expiresAt) ||
        !object.value(QStringLiteral("from_device_id")).isString() ||
        object.value(QStringLiteral("from_device_id")).toString() != expectedFrom ||
        !object.value(QStringLiteral("to_device_id")).isString() ||
        object.value(QStringLiteral("to_device_id")).toString() != expectedTo ||
        !object.value(QStringLiteral("signed_payload")).isString() ||
        !object.value(QStringLiteral("signature")).isString()) {
        error = QStringLiteral("Session description envelope fields are invalid.");
        return false;
    }

    const auto decodedPayload = decodeBase64Url(
        object.value(QStringLiteral("signed_payload")).toString().toLatin1());
    const auto decodedSignature = decodeBase64Url(
        object.value(QStringLiteral("signature")).toString().toLatin1());
    if (!decodedPayload || decodedPayload->isEmpty() ||
        decodedPayload->size() > kMaximumSignedPayloadBytes ||
        !decodedSignature || decodedSignature->size() != 64) {
        error = QStringLiteral("Session description authentication fields are invalid.");
        return false;
    }

    route.requestId = context.requestId;
    route.sessionId = context.sessionId;
    route.sessionEpoch = static_cast<quint32>(sessionEpoch);
    route.iceGeneration = static_cast<quint32>(iceGeneration);
    route.sequence = sequence;
    route.expiresAt = static_cast<qint64>(expiresAt);
    route.fromDeviceId = expectedFrom;
    route.toDeviceId = expectedTo;
    signedPayload = *decodedPayload;
    signature = *decodedSignature;
    return true;
}

bool decodeOfferPayload(const QByteArray &payload,
                        const SessionDescriptionContext &context,
                        SessionOfferFields &fields,
                        QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto type = reader.readText(64);
    const auto acceptDigest = reader.readBytes(32);
    const auto iceGeneration = reader.readUnsigned();
    const auto nonce = reader.readBytes(32);
    const auto sdp = reader.readText(kMaximumSessionDescriptionBytes);
    const auto sdpDigest = reader.readBytes(32);
    const auto fingerprint = reader.readBytes(32);
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 10 || !domain ||
        *domain != QString::fromLatin1(kSigningDomain) || !type ||
        *type != QString::fromLatin1(kOfferType) || !acceptDigest ||
        *acceptDigest != QCryptographicHash::hash(context.signedAcceptPayload,
                                                  QCryptographicHash::Sha256) ||
        !iceGeneration || *iceGeneration > 0xffffffffULL || !nonce ||
        nonce->size() != 32 || !sdp || !sdpDigest ||
        *sdpDigest != QCryptographicHash::hash(sdp->toUtf8(),
                                              QCryptographicHash::Sha256) ||
        !fingerprint || fingerprint->size() != 32 || !sequence ||
        *sequence < 2 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) ||
        !reader.atEnd()) {
        error = QStringLiteral("Session offer signed payload is invalid.");
        return false;
    }
    fields.signedAcceptPayload = context.signedAcceptPayload;
    fields.nonce = *nonce;
    fields.sdp = *sdp;
    return true;
}

bool decodeAnswerPayload(const QByteArray &payload,
                         const QByteArray &signedOfferPayload,
                         SessionAnswerFields &fields,
                         QString &error) {
    cbor::DeterministicCborReader reader(payload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(64);
    const auto type = reader.readText(64);
    const auto offerDigest = reader.readBytes(32);
    const auto iceGeneration = reader.readUnsigned();
    const auto sdp = reader.readText(kMaximumSessionDescriptionBytes);
    const auto sdpDigest = reader.readBytes(32);
    const auto fingerprint = reader.readBytes(32);
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 9 || !domain ||
        *domain != QString::fromLatin1(kSigningDomain) || !type ||
        *type != QString::fromLatin1(kAnswerType) || !offerDigest ||
        *offerDigest != QCryptographicHash::hash(signedOfferPayload,
                                                 QCryptographicHash::Sha256) ||
        !iceGeneration || *iceGeneration > 0xffffffffULL || !sdp ||
        !sdpDigest ||
        *sdpDigest != QCryptographicHash::hash(sdp->toUtf8(),
                                              QCryptographicHash::Sha256) ||
        !fingerprint || fingerprint->size() != 32 || !sequence ||
        *sequence < 2 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > static_cast<quint64>(kMaximumExactJsonInteger) ||
        !reader.atEnd()) {
        error = QStringLiteral("Session answer signed payload is invalid.");
        return false;
    }
    fields.signedOfferPayload = signedOfferPayload;
    fields.sdp = *sdp;
    return true;
}

}  // namespace

bool decodeSessionOfferEnvelope(const QByteArray &message,
                                const SessionDescriptionContext &context,
                                DecodedSessionOffer &offer,
                                QString &error) {
    offer = {};
    if (!contextIsValid(context)) {
        error = QStringLiteral("Session description context is invalid.");
        return false;
    }
    if (!decodeEnvelope(message,
                        context,
                        kOfferType,
                        context.offererDeviceId,
                        context.answererDeviceId,
                        offer.fields.route,
                        offer.signedPayload,
                        offer.signature,
                        error) ||
        !decodeOfferPayload(offer.signedPayload, context, offer.fields, error)) {
        return false;
    }
    QString canonicalError;
    if (sessionOfferSigningBytes(offer.fields, canonicalError) != offer.signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session offer payload is not canonical.")
                    : canonicalError;
        return false;
    }
    error.clear();
    return true;
}

bool decodeSessionAnswerEnvelope(const QByteArray &message,
                                 const SessionDescriptionContext &context,
                                 const QByteArray &signedOfferPayload,
                                 DecodedSessionAnswer &answer,
                                 QString &error) {
    answer = {};
    if (!contextIsValid(context) || signedOfferPayload.isEmpty() ||
        signedOfferPayload.size() > kMaximumSignedPayloadBytes) {
        error = QStringLiteral("Session description context is invalid.");
        return false;
    }
    if (!decodeEnvelope(message,
                        context,
                        kAnswerType,
                        context.answererDeviceId,
                        context.offererDeviceId,
                        answer.fields.route,
                        answer.signedPayload,
                        answer.signature,
                        error) ||
        !decodeAnswerPayload(answer.signedPayload,
                             signedOfferPayload,
                             answer.fields,
                             error)) {
        return false;
    }
    QString canonicalError;
    if (sessionAnswerSigningBytes(answer.fields, canonicalError) != answer.signedPayload) {
        error = canonicalError.isEmpty()
                    ? QStringLiteral("Session answer payload is not canonical.")
                    : canonicalError;
        return false;
    }
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
