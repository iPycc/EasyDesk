#include "network/signaling/protocol/session/session_request_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kSessionIdentifierSize = 16;
constexpr int kSessionPublicKeySize = 32;
constexpr int kSessionNonceSize = 32;
constexpr int kMaximumCapabilityGrants = 4;
constexpr auto kSessionSigningDomain = "EasyDesk-Session-v2";
constexpr auto kSessionMessageType = "session.request";
constexpr auto kSessionProtocolValue = "easydesk-session/2";

struct CanonicalSessionRequest {
    SessionRequestFields fields;
    QByteArray controllerPublicKey;
    QByteArray controlledPublicKey;
};

bool isDeviceId(const QString &value) {
    if (value.size() != 9)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool isKnownRole(SessionParticipantRole role) {
    return role == SessionParticipantRole::Controller ||
           role == SessionParticipantRole::Controlled;
}

bool isKnownCapability(SessionCapability capability) {
    return capability == SessionCapability::ViewScreen ||
           capability == SessionCapability::SendInput ||
           capability == SessionCapability::FileReceive;
}

bool byteArrayLess(const QByteArray &left, const QByteArray &right) {
    return std::lexicographical_compare(
        left.cbegin(), left.cend(), right.cbegin(), right.cend(),
        [](char leftByte, char rightByte) {
            return static_cast<unsigned char>(leftByte) <
                   static_cast<unsigned char>(rightByte);
        });
}

bool grantLess(const SessionCapabilityGrant &left,
               const SessionCapabilityGrant &right) {
    if (left.subjectPublicKey != right.subjectPublicKey)
        return byteArrayLess(left.subjectPublicKey, right.subjectPublicKey);
    return static_cast<quint8>(left.capability) <
           static_cast<quint8>(right.capability);
}

bool canonicalize(const SessionRequestFields &fields,
                  CanonicalSessionRequest &canonical,
                  QString &error) {
    canonical.fields = fields;
    if (!isKnownRole(fields.requesterRole)) {
        error = QStringLiteral("Session requester role is invalid.");
        return false;
    }
    canonical.controllerPublicKey =
        fields.requesterRole == SessionParticipantRole::Controller
            ? fields.requesterPublicKey
            : fields.targetPublicKey;
    canonical.controlledPublicKey =
        fields.requesterRole == SessionParticipantRole::Controlled
            ? fields.requesterPublicKey
            : fields.targetPublicKey;

    if (fields.requestId.size() != kSessionIdentifierSize ||
        fields.sessionId.size() != kSessionIdentifierSize) {
        error = QStringLiteral("Session request identifiers must contain exactly 16 bytes.");
        return false;
    }
    if (fields.requesterPublicKey.size() != kSessionPublicKeySize ||
        fields.targetPublicKey.size() != kSessionPublicKeySize ||
        fields.requesterPublicKey == fields.targetPublicKey) {
        error = QStringLiteral("Session participant public keys are invalid.");
        return false;
    }
    if (fields.nonce.size() != kSessionNonceSize) {
        error = QStringLiteral("Session request nonce must contain exactly 32 bytes.");
        return false;
    }
    if (fields.expiresAt <= 0 || fields.expiresAt > kMaximumExactJsonInteger) {
        error = QStringLiteral("Session request expiry is outside the allowed range.");
        return false;
    }
    if (!isDeviceId(fields.fromDeviceId) || !isDeviceId(fields.toDeviceId) ||
        fields.fromDeviceId == fields.toDeviceId) {
        error = QStringLiteral("Session request device route is invalid.");
        return false;
    }
    if (fields.requestedGrants.isEmpty() ||
        fields.requestedGrants.size() > kMaximumCapabilityGrants) {
        error = QStringLiteral("Session request capability count is invalid.");
        return false;
    }

    std::sort(canonical.fields.requestedGrants.begin(),
              canonical.fields.requestedGrants.end(), grantLess);
    bool controllerMayView = false;
    bool controllerMaySendInput = false;
    for (int index = 0; index < canonical.fields.requestedGrants.size(); ++index) {
        const SessionCapabilityGrant &grant = canonical.fields.requestedGrants.at(index);
        const bool participant = grant.subjectPublicKey == canonical.controllerPublicKey ||
                                 grant.subjectPublicKey == canonical.controlledPublicKey;
        const bool controllerOnly = grant.capability == SessionCapability::ViewScreen ||
                                    grant.capability == SessionCapability::SendInput;
        if (!isKnownCapability(grant.capability) || !participant ||
            (controllerOnly && grant.subjectPublicKey != canonical.controllerPublicKey) ||
            (index > 0 && !grantLess(canonical.fields.requestedGrants.at(index - 1),
                                    grant))) {
            error = QStringLiteral("Session request capabilities are invalid.");
            return false;
        }
        controllerMayView = controllerMayView ||
                            grant.capability == SessionCapability::ViewScreen;
        controllerMaySendInput = controllerMaySendInput ||
                                 grant.capability == SessionCapability::SendInput;
    }
    if (controllerMaySendInput && !controllerMayView) {
        error = QStringLiteral("Input control requires screen viewing permission.");
        return false;
    }
    error.clear();
    return true;
}

QByteArray encodePayload(const CanonicalSessionRequest &canonical) {
    cbor::DeterministicCborWriter writer;
    writer.beginArray(10);
    writer.writeText(QString::fromLatin1(kSessionSigningDomain));
    writer.writeText(QString::fromLatin1(kSessionMessageType));
    writer.writeBytes(canonical.fields.requestId);
    writer.writeBytes(canonical.fields.sessionId);
    writer.writeBytes(canonical.fields.requesterPublicKey);
    writer.writeBytes(canonical.fields.targetPublicKey);
    writer.writeUnsigned(static_cast<quint8>(canonical.fields.requesterRole));
    writer.writeBytes(canonical.fields.nonce);
    writer.beginArray(static_cast<quint64>(canonical.fields.requestedGrants.size()));
    for (const SessionCapabilityGrant &grant : canonical.fields.requestedGrants) {
        writer.beginArray(2);
        writer.writeBytes(grant.subjectPublicKey);
        writer.writeUnsigned(static_cast<quint8>(grant.capability));
    }
    writer.writeUnsigned(static_cast<quint64>(canonical.fields.expiresAt));
    return writer.takeBytes();
}

}  // namespace

QByteArray sessionRequestSigningBytes(const SessionRequestFields &fields,
                                      QString &error) {
    CanonicalSessionRequest canonical;
    if (!canonicalize(fields, canonical, error))
        return {};
    return encodePayload(canonical);
}

QByteArray encodeSessionRequestEnvelope(const SessionRequestFields &fields,
                                        const QByteArray &signature,
                                        QString &error) {
    CanonicalSessionRequest canonical;
    if (!canonicalize(fields, canonical, error))
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session request signature must contain exactly 64 bytes.");
        return {};
    }

    const QByteArray signedPayload = encodePayload(canonical);
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kSessionProtocolValue));
    object.insert(QStringLiteral("type"), QString::fromLatin1(kSessionMessageType));
    object.insert(QStringLiteral("request_id"),
                  QString::fromLatin1(encodeBase64Url(fields.requestId)));
    object.insert(QStringLiteral("session_id"),
                  QString::fromLatin1(encodeBase64Url(fields.sessionId)));
    object.insert(QStringLiteral("seq"), 1);
    object.insert(QStringLiteral("expires_at"), static_cast<double>(fields.expiresAt));
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
