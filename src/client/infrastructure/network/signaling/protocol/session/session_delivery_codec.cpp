#include "network/signaling/protocol/session/session_delivery_codec.h"

#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace easydesk::client::signaling {
namespace {

constexpr qint64 kMaximumExactJsonInteger = 9007199254740991LL;
constexpr auto kSessionProtocolValue = "easydesk-session/2";

bool isKnownMessageType(const QString &value) {
    return value == QStringLiteral("session.request") ||
           value == QStringLiteral("session.accept") ||
           value == QStringLiteral("session.reject") ||
           value == QStringLiteral("session.cancel") ||
           value == QStringLiteral("session.offer") ||
           value == QStringLiteral("session.answer") ||
           value == QStringLiteral("session.candidates") ||
           value == QStringLiteral("session.gathering_complete") ||
           value == QStringLiteral("session.end") ||
           value == QStringLiteral("session.revoke");
}

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

}  // namespace

bool decodeSessionDelivery(const QByteArray &message,
                           SessionDeliveryMessage &delivery,
                           QString &error) {
    delivery = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Session delivery receipt is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue protocol = object.value(QStringLiteral("protocol"));
    const QJsonValue messageType = object.value(QStringLiteral("message_type"));
    const bool hasSessionEpoch = object.contains(QStringLiteral("session_epoch"));
    if (object.size() != (hasSessionEpoch ? 7 : 6) ||
        !type.isString() || type.toString() != QStringLiteral("session.delivered") ||
        !protocol.isString() ||
        protocol.toString() != QString::fromLatin1(kSessionProtocolValue) ||
        !messageType.isString() || !isKnownMessageType(messageType.toString()) ||
        !decodeIdentifier(object.value(QStringLiteral("request_id")), delivery.requestId) ||
        !decodeIdentifier(object.value(QStringLiteral("session_id")), delivery.sessionId) ||
        !exactUnsigned(object.value(QStringLiteral("seq")), 1,
                       static_cast<quint64>(kMaximumExactJsonInteger),
                       delivery.sequence)) {
        error = QStringLiteral("Session delivery receipt fields are invalid.");
        delivery = {};
        return false;
    }

    if (hasSessionEpoch) {
        quint64 epoch = 0;
        if (!exactUnsigned(object.value(QStringLiteral("session_epoch")), 1,
                           0xffffffffULL, epoch)) {
            error = QStringLiteral("Session delivery epoch is invalid.");
            delivery = {};
            return false;
        }
        delivery.hasSessionEpoch = true;
        delivery.sessionEpoch = static_cast<quint32>(epoch);
    }
    delivery.messageType = messageType.toString();
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
