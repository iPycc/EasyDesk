#include "network/signaling/protocol/session/ice/session_ice_codec.h"

#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumBoundPayloadBytes = 44 * 1024;
constexpr int kMaximumCandidateBatchSize = 8;
constexpr int kMaximumMidBytes = 64;
constexpr int kMaximumUfragBytes = 256;
constexpr int kMaximumCandidateBytes = 2048;
constexpr auto kSigningDomain = "EasyDesk-Session-v2";
constexpr auto kProtocol = "easydesk-session/2";
constexpr auto kCandidatesType = "session.candidates";
constexpr auto kGatheringCompleteType = "session.gathering_complete";

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
        route.sessionEpoch == 0 || route.sequence < 3 ||
        route.sequence > static_cast<quint64>(kMaximumExactJsonInteger) ||
        route.expiresAt <= 0 || route.expiresAt > kMaximumExactJsonInteger ||
        !containsDigits(route.fromDeviceId, 9) ||
        !containsDigits(route.toDeviceId, 9) ||
        route.fromDeviceId == route.toDeviceId) {
        error = QStringLiteral("Session ICE route is invalid.");
        return false;
    }
    return true;
}

bool singleLine(const QString &value, int maximumBytes) {
    const QByteArray utf8 = value.toUtf8();
    return !utf8.isEmpty() && utf8.size() <= maximumBytes &&
           !value.contains(QChar::Null) && !value.contains(QLatin1Char('\r')) &&
           !value.contains(QLatin1Char('\n'));
}

bool validUfrag(const QString &value) {
    if (!singleLine(value, kMaximumUfragBytes))
        return false;
    const QByteArray bytes = value.toLatin1();
    if (QString::fromLatin1(bytes) != value)
        return false;
    for (const char byte : bytes) {
        const bool alphaNumeric =
            (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9');
        if (!alphaNumeric && byte != '+' && byte != '/')
            return false;
    }
    return true;
}

bool sameCandidate(const SessionIceCandidate &left,
                   const SessionIceCandidate &right) {
    return left.sdpMid == right.sdpMid &&
           left.sdpMLineIndex == right.sdpMLineIndex &&
           left.usernameFragment == right.usernameFragment &&
           left.candidate == right.candidate;
}

bool validCandidates(const QVector<SessionIceCandidate> &candidates,
                     QString &error) {
    if (candidates.isEmpty() || candidates.size() > kMaximumCandidateBatchSize) {
        error = QStringLiteral("Session ICE candidate count is invalid.");
        return false;
    }
    for (int index = 0; index < candidates.size(); ++index) {
        const auto &candidate = candidates.at(index);
        if (!singleLine(candidate.sdpMid, kMaximumMidBytes) ||
            candidate.sdpMLineIndex > 255 ||
            !validUfrag(candidate.usernameFragment) ||
            !singleLine(candidate.candidate, kMaximumCandidateBytes) ||
            !candidate.candidate.startsWith(QStringLiteral("candidate:"))) {
            error = QStringLiteral("Session ICE candidate is invalid.");
            return false;
        }
        for (int previous = 0; previous < index; ++previous) {
            if (sameCandidate(candidate, candidates.at(previous))) {
                error = QStringLiteral("Session ICE candidate batch contains duplicates.");
                return false;
            }
        }
    }
    return true;
}

bool validBinding(const QByteArray &payload, QString &error) {
    if (payload.isEmpty() || payload.size() > kMaximumBoundPayloadBytes) {
        error = QStringLiteral("Session ICE description binding is invalid.");
        return false;
    }
    return true;
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

QByteArray sessionCandidatesSigningBytes(const SessionCandidatesFields &fields,
                                         QString &error) {
    error.clear();
    if (!validRoute(fields.route, error) ||
        !validBinding(fields.signedDescriptionPayload, error) ||
        !validCandidates(fields.candidates, error)) {
        return {};
    }
    cbor::DeterministicCborWriter writer;
    writer.beginArray(7);
    writer.writeText(QString::fromLatin1(kSigningDomain));
    writer.writeText(QString::fromLatin1(kCandidatesType));
    writer.writeBytes(QCryptographicHash::hash(fields.signedDescriptionPayload,
                                               QCryptographicHash::Sha256));
    writer.writeUnsigned(fields.route.iceGeneration);
    writer.beginArray(static_cast<quint64>(fields.candidates.size()));
    for (const auto &candidate : fields.candidates) {
        writer.beginArray(4);
        writer.writeText(candidate.sdpMid);
        writer.writeUnsigned(candidate.sdpMLineIndex);
        writer.writeText(candidate.usernameFragment);
        writer.writeText(candidate.candidate);
    }
    writer.writeUnsigned(fields.route.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.route.expiresAt));
    error.clear();
    return writer.takeBytes();
}

QByteArray encodeSessionCandidatesEnvelope(const SessionCandidatesFields &fields,
                                           const QByteArray &signature,
                                           QString &error) {
    const QByteArray signedPayload = sessionCandidatesSigningBytes(fields, error);
    if (signedPayload.isEmpty())
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session ICE candidate signature is invalid.");
        return {};
    }
    error.clear();
    return encodeEnvelope(fields.route, kCandidatesType, signedPayload, signature);
}

QByteArray sessionGatheringCompleteSigningBytes(
    const SessionGatheringCompleteFields &fields,
    QString &error) {
    error.clear();
    if (!validRoute(fields.route, error) ||
        !validBinding(fields.signedDescriptionPayload, error)) {
        return {};
    }
    cbor::DeterministicCborWriter writer;
    writer.beginArray(6);
    writer.writeText(QString::fromLatin1(kSigningDomain));
    writer.writeText(QString::fromLatin1(kGatheringCompleteType));
    writer.writeBytes(QCryptographicHash::hash(fields.signedDescriptionPayload,
                                               QCryptographicHash::Sha256));
    writer.writeUnsigned(fields.route.iceGeneration);
    writer.writeUnsigned(fields.route.sequence);
    writer.writeUnsigned(static_cast<quint64>(fields.route.expiresAt));
    error.clear();
    return writer.takeBytes();
}

QByteArray encodeSessionGatheringCompleteEnvelope(
    const SessionGatheringCompleteFields &fields,
    const QByteArray &signature,
    QString &error) {
    const QByteArray signedPayload =
        sessionGatheringCompleteSigningBytes(fields, error);
    if (signedPayload.isEmpty())
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Session ICE gathering signature is invalid.");
        return {};
    }
    error.clear();
    return encodeEnvelope(fields.route,
                          kGatheringCompleteType,
                          signedPayload,
                          signature);
}

}  // namespace easydesk::client::signaling
