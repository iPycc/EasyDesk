#include "network/signaling/protocol/session/session_accept_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionMessageType = "session.accept";
constexpr auto kSessionProtocolValue = "easydesk-session/2";

struct CanonicalSessionAccept {
    SessionAcceptFields fields;
    QByteArray requestDigest;
};

bool canonicalize(const SessionAcceptFields &fields,
                  CanonicalSessionAccept &canonical,
                  QString &error) {
    canonical.fields = fields;
    const QByteArray expectedRequestPayload =
        sessionRequestSigningBytes(canonical.fields.request, error);
    if (expectedRequestPayload.isEmpty() ||
        expectedRequestPayload != canonical.fields.signedRequestPayload) {
        if (error.isEmpty())
            error = QStringLiteral("Accepted session request payload is not canonical.");
        return false;
    }

    const QByteArray controllerPublicKey =
        fields.request.requesterRole == SessionParticipantRole::Controller
            ? fields.request.requesterPublicKey
            : fields.request.targetPublicKey;
    const QByteArray controlledPublicKey =
        fields.request.requesterRole == SessionParticipantRole::Controlled
            ? fields.request.requesterPublicKey
            : fields.request.targetPublicKey;
    QVector<SessionCapabilityGrant> requestedGrants = fields.request.requestedGrants;
    if (!canonicalizeSessionCapabilityGrants(requestedGrants,
                                             controllerPublicKey,
                                             controlledPublicKey,
                                             error) ||
        !canonicalizeSessionCapabilityGrants(canonical.fields.grantedGrants,
                                             controllerPublicKey,
                                             controlledPublicKey,
                                             error) ||
        !sessionCapabilityGrantsAreSubset(canonical.fields.grantedGrants,
                                         requestedGrants)) {
        if (error.isEmpty())
            error = QStringLiteral("Granted session capabilities exceed the request.");
        return false;
    }
    if (fields.nonce.size() != 32) {
        error = QStringLiteral("Session acceptance nonce must contain exactly 32 bytes.");
        return false;
    }
    if (fields.sessionEpoch == 0 || fields.permissionEpoch == 0) {
        error = QStringLiteral("Session and permission epochs must be positive.");
        return false;
    }
    if (fields.expiresAt <= 0 || fields.expiresAt > kMaximumExactJsonInteger) {
        error = QStringLiteral("Session acceptance expiry is outside the allowed range.");
        return false;
    }

    canonical.requestDigest = QCryptographicHash::hash(
        canonical.fields.signedRequestPayload, QCryptographicHash::Sha256);
    error.clear();
    return true;
}

QByteArray encodePayload(const CanonicalSessionAccept &canonical) {
    cbor::DeterministicCborWriter writer;
    writer.beginArray(8);
    writer.writeText(QString::fromLatin1(kSessionSigningDomain));
    writer.writeText(QString::fromLatin1(kSessionMessageType));
    writer.writeBytes(canonical.requestDigest);
    writer.writeBytes(canonical.fields.nonce);
    writer.beginArray(static_cast<quint64>(canonical.fields.grantedGrants.size()));
    for (const SessionCapabilityGrant &grant : canonical.fields.grantedGrants) {
        writer.beginArray(2);
        writer.writeBytes(grant.subjectPublicKey);
        writer.writeUnsigned(static_cast<quint8>(grant.capability));
    }
    writer.writeUnsigned(canonical.fields.sessionEpoch);
    writer.writeUnsigned(canonical.fields.permissionEpoch);
    writer.writeUnsigned(static_cast<quint64>(canonical.fields.expiresAt));
    return writer.takeBytes();
}

}  // namespace

QByteArray sessionAcceptSigningBytes(const SessionAcceptFields &fields,
                                     QString &error) {
    CanonicalSessionAccept canonical;
    if (!canonicalize(fields, canonical, error))
        return {};
    return encodePayload(canonical);
}

QByteArray encodeSessionAcceptEnvelope(const SessionAcceptFields &fields,
                                       const QByteArray &signature,
                                       QString &error) {
    CanonicalSessionAccept canonical;
    if (!canonicalize(fields, canonical, error))
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session acceptance signature must contain exactly 64 bytes.");
        return {};
    }

    const QByteArray signedPayload = encodePayload(canonical);
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kSessionProtocolValue));
    object.insert(QStringLiteral("type"), QString::fromLatin1(kSessionMessageType));
    object.insert(QStringLiteral("request_id"),
                  QString::fromLatin1(encodeBase64Url(fields.request.requestId)));
    object.insert(QStringLiteral("session_id"),
                  QString::fromLatin1(encodeBase64Url(fields.request.sessionId)));
    object.insert(QStringLiteral("session_epoch"),
                  static_cast<double>(fields.sessionEpoch));
    object.insert(QStringLiteral("seq"), 1);
    object.insert(QStringLiteral("expires_at"), static_cast<double>(fields.expiresAt));
    object.insert(QStringLiteral("from_device_id"), fields.request.toDeviceId);
    object.insert(QStringLiteral("to_device_id"), fields.request.fromDeviceId);
    object.insert(QStringLiteral("signed_payload"),
                  QString::fromLatin1(encodeBase64Url(signedPayload)));
    object.insert(QStringLiteral("signature"),
                  QString::fromLatin1(encodeBase64Url(signature)));
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace easydesk::client::signaling
