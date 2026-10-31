#include "network/signaling/protocol/turn/turn_credentials_decoder.h"

#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr int kMaximumTurnUrls = 8;
constexpr int kMaximumTurnUrlBytes = 512;
constexpr int kMaximumUsernameBytes = 128;
constexpr int kMaximumPasswordBytes = 256;
constexpr auto kMessageType = "turn.credentials";
constexpr auto kProtocol = "easydesk-session/2";

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

bool isVisibleAscii(const QString &value, int maximumBytes) {
    const QByteArray bytes = value.toUtf8();
    if (bytes.isEmpty() || bytes.size() > maximumBytes)
        return false;
    for (const char byte : bytes) {
        const unsigned char character = static_cast<unsigned char>(byte);
        if (character < 0x21 || character > 0x7e)
            return false;
    }
    return true;
}

bool isTurnUrl(const QString &value) {
    const QByteArray bytes = value.toUtf8();
    if (bytes.isEmpty() || bytes.size() > kMaximumTurnUrlBytes ||
        (!value.startsWith(QStringLiteral("turn:"), Qt::CaseSensitive) &&
         !value.startsWith(QStringLiteral("turns:"), Qt::CaseSensitive))) {
        return false;
    }
    for (const QChar character : value) {
        if (character.isSpace() || character.isNull())
            return false;
    }
    return true;
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

bool usernameMatchesSession(const QString &username,
                            qint64 expiresAt,
                            const QByteArray &sessionId) {
    const QStringList parts = username.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    if (parts.size() != 3 || parts.at(0) != QString::number(expiresAt) ||
        !containsDigits(parts.at(1), 9)) {
        return false;
    }
    return parts.at(2).toLatin1() == encodeBase64Url(sessionId);
}

bool isCanonicalSha1Password(const QString &password) {
    if (!isVisibleAscii(password, kMaximumPasswordBytes))
        return false;
    const QByteArray encoded = password.toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(encoded);
    return decoded.size() == 20 && decoded.toBase64() == encoded;
}

}  // namespace

bool decodeTurnCredentials(const QByteArray &message,
                           TurnCredentialsMessage &credentials,
                           QString &error) {
    credentials = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("TURN credentials are not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    quint64 sessionEpoch = 0;
    quint64 expiresAt = 0;
    if (object.size() != 9 ||
        !object.value(QStringLiteral("type")).isString() ||
        object.value(QStringLiteral("type")).toString() !=
            QString::fromLatin1(kMessageType) ||
        !object.value(QStringLiteral("protocol")).isString() ||
        object.value(QStringLiteral("protocol")).toString() !=
            QString::fromLatin1(kProtocol) ||
        !decodeIdentifier(object.value(QStringLiteral("request_id")),
                          credentials.requestId) ||
        !decodeIdentifier(object.value(QStringLiteral("session_id")),
                          credentials.sessionId) ||
        !exactUnsigned(object.value(QStringLiteral("session_epoch")),
                       1, 0xffffffffULL, sessionEpoch) ||
        !exactUnsigned(object.value(QStringLiteral("expires_at")),
                       1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       expiresAt)) {
        error = QStringLiteral("TURN credential session fields are invalid.");
        return false;
    }

    const QJsonValue urlsValue = object.value(QStringLiteral("urls"));
    if (!urlsValue.isArray()) {
        error = QStringLiteral("TURN credential URLs are missing.");
        return false;
    }
    const QJsonArray urls = urlsValue.toArray();
    if (urls.isEmpty() || urls.size() > kMaximumTurnUrls) {
        error = QStringLiteral("TURN credential URL count is invalid.");
        return false;
    }
    for (const QJsonValue &url : urls) {
        if (!url.isString() || !isTurnUrl(url.toString())) {
            error = QStringLiteral("TURN credential URL is invalid.");
            return false;
        }
        credentials.urls.append(url.toString());
    }

    const QJsonValue usernameValue = object.value(QStringLiteral("username"));
    const QJsonValue passwordValue = object.value(QStringLiteral("password"));
    if (!usernameValue.isString() ||
        !isVisibleAscii(usernameValue.toString(), kMaximumUsernameBytes) ||
        !passwordValue.isString() ||
        !isCanonicalSha1Password(passwordValue.toString())) {
        error = QStringLiteral("TURN credential authentication fields are invalid.");
        return false;
    }

    credentials.sessionEpoch = static_cast<quint32>(sessionEpoch);
    credentials.expiresAt = static_cast<qint64>(expiresAt);
    credentials.username = usernameValue.toString();
    credentials.password = passwordValue.toString();
    if (!usernameMatchesSession(credentials.username,
                                credentials.expiresAt,
                                credentials.sessionId)) {
        error = QStringLiteral("TURN credential username does not match the session.");
        credentials = {};
        return false;
    }

    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
