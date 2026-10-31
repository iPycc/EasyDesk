#include "network/signaling/protocol/registration/registration_codec.h"

#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cmath>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr char kRegistrationDomain[] = "EasyDesk-Register-v1";

struct CanonicalRegistrationFields {
    QByteArray deviceId;
    QByteArray publicKey;
    QString displayNameText;
    QByteArray displayName;
    QString platformText;
    QByteArray platform;
    quint32 schemaVersion = 0;
    quint64 timestamp = 0;
    QString nonceText;
    QByteArray nonce;
};

bool isDeviceId(const QByteArray &value) {
    if (value.size() != 9)
        return false;
    for (const char character : value) {
        if (character < '0' || character > '9')
            return false;
    }
    return true;
}

bool canonicalize(const RegistrationFields &fields,
                  CanonicalRegistrationFields &canonical,
                  QString &error) {
    canonical.deviceId = fields.deviceId.toUtf8();
    canonical.publicKey = fields.publicKey;
    canonical.displayNameText =
        fields.displayName.normalized(QString::NormalizationForm_C);
    canonical.displayName = canonical.displayNameText.toUtf8();
    canonical.platformText = fields.platform.normalized(QString::NormalizationForm_C);
    canonical.platform = canonical.platformText.toUtf8();
    canonical.schemaVersion = fields.schemaVersion;
    canonical.timestamp = static_cast<quint64>(fields.timestamp);
    canonical.nonceText = fields.nonce;
    canonical.nonce = fields.nonce.toUtf8();

    const auto decodedNonce = decodeBase64Url(canonical.nonce);
    if (!isDeviceId(canonical.deviceId)) {
        error = QStringLiteral("Device ID must contain exactly 9 digits.");
    } else if (canonical.publicKey.size() != 32) {
        error = QStringLiteral("Registration public key must contain exactly 32 bytes.");
    } else if (canonical.displayName.isEmpty() || canonical.displayName.size() > 128) {
        error = QStringLiteral("Device name must contain between 1 and 128 UTF-8 bytes.");
    } else if (canonical.platform.isEmpty() || canonical.platform.size() > 64) {
        error = QStringLiteral("Platform must contain between 1 and 64 UTF-8 bytes.");
    } else if (canonical.schemaVersion < 1 || canonical.schemaVersion > 65535) {
        error = QStringLiteral("Registration schema version is outside the allowed range.");
    } else if (fields.timestamp <= 0 || fields.timestamp > kMaximumExactJsonInteger) {
        error = QStringLiteral("Registration timestamp is outside the exact JSON integer range.");
    } else if (canonical.nonce.isEmpty() || canonical.nonce.size() > 128 ||
               !decodedNonce || decodedNonce->size() != 32) {
        error = QStringLiteral("Registration challenge nonce is invalid.");
    } else {
        error.clear();
        return true;
    }
    return false;
}

void appendU32(QByteArray &output, quint32 value) {
    output.append(static_cast<char>((value >> 24U) & 0xffU));
    output.append(static_cast<char>((value >> 16U) & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
    output.append(static_cast<char>(value & 0xffU));
}

void appendU64(QByteArray &output, quint64 value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        output.append(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xffU));
}

void appendBytes(QByteArray &output, const QByteArray &value) {
    appendU32(output, static_cast<quint32>(value.size()));
    output.append(value);
}

}  // namespace

bool parseRegistrationChallenge(const QByteArray &message,
                                RegistrationChallenge &challenge,
                                QString &error) {
    challenge = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Signaling challenge is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue protocol = object.value(QStringLiteral("protocol"));
    const QJsonValue nonce = object.value(QStringLiteral("nonce"));
    const QJsonValue expiresAt = object.value(QStringLiteral("expires_at"));
    if (!type.isString() || type.toString() != QStringLiteral("register.challenge") ||
        !protocol.isString() || protocol.toString() != QString::fromLatin1(kSignalingProtocol) ||
        !nonce.isString() || !expiresAt.isDouble()) {
        error = QStringLiteral("Signaling challenge fields are missing or incompatible.");
        return false;
    }

    const QByteArray nonceBytes = nonce.toString().toUtf8();
    const auto decodedNonce = decodeBase64Url(nonceBytes);
    const double expiration = expiresAt.toDouble();
    if (!decodedNonce || decodedNonce->size() != 32 || nonceBytes.size() > 128 ||
        !std::isfinite(expiration) || expiration <= 0 ||
        expiration > static_cast<double>(kMaximumExactJsonInteger) ||
        std::floor(expiration) != expiration) {
        error = QStringLiteral("Signaling challenge nonce or expiry is invalid.");
        return false;
    }

    challenge.nonce = nonce.toString();
    challenge.expiresAt = static_cast<qint64>(expiration);
    error.clear();
    return true;
}

QByteArray registrationSigningBytes(const RegistrationFields &fields,
                                    QString &error) {
    CanonicalRegistrationFields canonical;
    if (!canonicalize(fields, canonical, error))
        return {};

    QByteArray output;
    output.reserve(128 + canonical.displayName.size() + canonical.platform.size());
    output.append(kRegistrationDomain, static_cast<int>(sizeof(kRegistrationDomain) - 1));
    output.append('\0');
    appendBytes(output, canonical.deviceId);
    appendBytes(output, canonical.publicKey);
    appendBytes(output, canonical.displayName);
    appendBytes(output, canonical.platform);
    appendU32(output, canonical.schemaVersion);
    appendU64(output, canonical.timestamp);
    appendBytes(output, canonical.nonce);
    error.clear();
    return output;
}

QByteArray encodeRegistrationResponse(const RegistrationFields &fields,
                                      const QByteArray &signature,
                                      QString &error) {
    CanonicalRegistrationFields canonical;
    if (!canonicalize(fields, canonical, error))
        return {};
    if (signature.size() != 64) {
        error = QStringLiteral("Registration signature must contain exactly 64 bytes.");
        return {};
    }

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("register.response"));
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kSignalingProtocol));
    object.insert(QStringLiteral("device_id"), QString::fromLatin1(canonical.deviceId));
    object.insert(QStringLiteral("public_key"),
                  QString::fromLatin1(encodeBase64Url(canonical.publicKey)));
    object.insert(QStringLiteral("display_name"), canonical.displayNameText);
    object.insert(QStringLiteral("platform"), canonical.platformText);
    object.insert(QStringLiteral("schema_version"), static_cast<int>(canonical.schemaVersion));
    object.insert(QStringLiteral("timestamp"), static_cast<double>(canonical.timestamp));
    object.insert(QStringLiteral("nonce"), canonical.nonceText);
    object.insert(QStringLiteral("signature"),
                  QString::fromLatin1(encodeBase64Url(signature)));
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace easydesk::client::signaling
