#include "network/signaling/protocol/session/description/session_description_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"
#include "network/signaling/protocol/session/description/dtls_fingerprint.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumBoundPayloadBytes = 44 * 1024;
constexpr auto kSigningDomain = "EasyDesk-Session-v2";
constexpr auto kProtocol = "easydesk-session/2";
constexpr auto kOfferType = "session.offer";
constexpr auto kAnswerType = "session.answer";

bool containsDigits(const QString &value, int length) {
    if (value.size() != length)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool validRoute(const SessionDescriptionRoute &route, QString &error) {
    if (route.requestId.size() != 16 || route.sessionId.size() != 16 ||
        route.sessionEpoch == 0 || route.sequence < 2 ||
        route.sequence > static_cast<quint64>(kMaximumExactJsonInteger) ||
        route.expiresAt <= 0 || route.expiresAt > kMaximumExactJsonInteger ||
        !containsDigits(route.fromDeviceId, 9) ||
        !containsDigits(route.toDeviceId, 9) ||
        route.fromDeviceId == route.toDeviceId) {
        error = QStringLiteral("Session description route is invalid.");
        return false;
    }
    return true;
}

bool validBoundPayload(const QByteArray &payload, QString &error) {
    if (payload.isEmpty() || payload.size() > kMaximumBoundPayloadBytes) {
        error = QStringLiteral("Session description binding payload is invalid.");
        return false;
    }
    return true;
}

QByteArray encodeOfferPayload(const SessionOfferFields &fields,
                              const QByteArray &fingerprint) {
    const QByteArray sdp = fields.sdp.toUtf8();
    cbor::DeterministicCborWriter writer;
    writer.beginArray(10);
    writer.writeText(QString::fromLatin1(kSigningDomain));
    writer.writeText(QString::fromLatin1(kOfferType));
    writer.writeBytes(QCryptographicHash::hash(fields.signedAcceptPayload,
                                               QCryptographicHash::Sha256));
    writer.writeUnsigned(fields.route.iceGeneration);
    writer.writeBytes(fields.nonce);
    writer.writeText(fields.sdp);
    writer.writeBytes(QCryptographicHash::hash(sdp, QCryptographicHash::Sha256));
    writer.writeBytes(fingerprint);
    writer.writeUnsigned(fields.route.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.route.expiresAt));
    return writer.takeBytes();
}

QByteArray encodeAnswerPayload(const SessionAnswerFields &fields,
                               const QByteArray &fingerprint) {
    const QByteArray sdp = fields.sdp.toUtf8();
    cbor::DeterministicCborWriter writer;
    writer.beginArray(9);
    writer.writeText(QString::fromLatin1(kSigningDomain));
    writer.writeText(QString::fromLatin1(kAnswerType));
    writer.writeBytes(QCryptographicHash::hash(fields.signedOfferPayload,
                                               QCryptographicHash::Sha256));
    writer.writeUnsigned(fields.route.iceGeneration);
    writer.writeText(fields.sdp);
    writer.writeBytes(QCryptographicHash::hash(sdp, QCryptographicHash::Sha256));
    writer.writeBytes(fingerprint);
    writer.writeUnsigned(fields.route.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.route.expiresAt));
    return writer.takeBytes();
}

QByteArray encodeEnvelope(const SessionDescriptionRoute &route,
                          const char *type,
                          const QByteArray &signedPayload,
                          const QByteArray &signature) {
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kProtocol));
    object.insert(QStringLiteral("type"), QString::fromLatin1(type));
    object.insert(QStringLiteral("request_id"),
                  QString::fromLatin1(encodeBase64Url(route.requestId)));
    object.insert(QStringLiteral("session_id"),
                  QString::fromLatin1(encodeBase64Url(route.sessionId)));
    object.insert(QStringLiteral("session_epoch"),
                  static_cast<double>(route.sessionEpoch));
    object.insert(QStringLiteral("ice_generation"),
                  static_cast<double>(route.iceGeneration));
    object.insert(QStringLiteral("seq"), static_cast<double>(route.sequence));
    object.insert(QStringLiteral("expires_at"),
                  static_cast<double>(route.expiresAt));
    object.insert(QStringLiteral("from_device_id"), route.fromDeviceId);
    object.insert(QStringLiteral("to_device_id"), route.toDeviceId);
    object.insert(QStringLiteral("signed_payload"),
                  QString::fromLatin1(encodeBase64Url(signedPayload)));
    object.insert(QStringLiteral("signature"),
                  QString::fromLatin1(encodeBase64Url(signature)));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

QByteArray sessionOfferSigningBytes(const SessionOfferFields &fields,
                                    QString &error) {
    error.clear();
    QByteArray fingerprint;
    if (!validRoute(fields.route, error) ||
        !validBoundPayload(fields.signedAcceptPayload, error) ||
        fields.nonce.size() != 32 ||
        !extractSha256DtlsFingerprint(fields.sdp, fingerprint, error)) {
        if (error.isEmpty())
            error = QStringLiteral("Session offer nonce is invalid.");
        return {};
    }
    error.clear();
    return encodeOfferPayload(fields, fingerprint);
}

QByteArray encodeSessionOfferEnvelope(const SessionOfferFields &fields,
                                      const QByteArray &signature,
                                      QString &error) {
    const QByteArray signedPayload = sessionOfferSigningBytes(fields, error);
    if (signedPayload.isEmpty())
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session offer signature is invalid.");
        return {};
    }
    error.clear();
    return encodeEnvelope(fields.route, kOfferType, signedPayload, signature);
}

QByteArray sessionAnswerSigningBytes(const SessionAnswerFields &fields,
                                     QString &error) {
    error.clear();
    QByteArray fingerprint;
    if (!validRoute(fields.route, error) ||
        !validBoundPayload(fields.signedOfferPayload, error) ||
        !extractSha256DtlsFingerprint(fields.sdp, fingerprint, error)) {
        return {};
    }
    error.clear();
    return encodeAnswerPayload(fields, fingerprint);
}

QByteArray encodeSessionAnswerEnvelope(const SessionAnswerFields &fields,
                                       const QByteArray &signature,
                                       QString &error) {
    const QByteArray signedPayload = sessionAnswerSigningBytes(fields, error);
    if (signedPayload.isEmpty())
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session answer signature is invalid.");
        return {};
    }
    error.clear();
    return encodeEnvelope(fields.route, kAnswerType, signedPayload, signature);
}

}  // namespace easydesk::client::signaling
