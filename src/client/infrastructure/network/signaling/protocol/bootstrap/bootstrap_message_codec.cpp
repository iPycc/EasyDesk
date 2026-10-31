#include "network/signaling/protocol/bootstrap/bootstrap_message_codec.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cmath>

namespace easydesk::client::signaling {
namespace {

bool containsDigits(const QString &value, int length) {
    if (value.size() != length)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool isFingerprint(const QString &value) {
    if (value.size() != 64)
        return false;
    for (const QChar character : value) {
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        const bool lowerHex = character >= QLatin1Char('a') && character <= QLatin1Char('f');
        if (!digit && !lowerHex)
            return false;
    }
    return true;
}

bool exactInteger(const QJsonValue &value, qint64 minimum, qint64 maximum, qint64 &result) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(minimum) || number > static_cast<double>(maximum)) {
        return false;
    }
    result = static_cast<qint64>(number);
    return true;
}

bool textField(const QJsonObject &object,
               const QString &name,
               int maximumBytes,
               QString &result) {
    const QJsonValue value = object.value(name);
    if (!value.isString())
        return false;
    result = value.toString();
    return !result.isEmpty() && result.toUtf8().size() <= maximumBytes;
}

}  // namespace

bool decodeBootstrapMessage(const QByteArray &message,
                            BootstrapMessage &decoded,
                            QString &error) {
    decoded = {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Signaling message is not a valid JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    if (!textField(object, QStringLiteral("type"), 64, decoded.type)) {
        error = QStringLiteral("Signaling message type is missing or invalid.");
        return false;
    }

    if (decoded.type == QStringLiteral("register.challenge")) {
        decoded.kind = BootstrapMessage::Kind::RegistrationChallenge;
        return parseRegistrationChallenge(message, decoded.challenge, error);
    }
    if (decoded.type == QStringLiteral("register.result")) {
        decoded.kind = BootstrapMessage::Kind::RegistrationResult;
        if (!textField(object, QStringLiteral("status"), 16,
                       decoded.registration.status) ||
            (decoded.registration.status != QStringLiteral("created") &&
             decoded.registration.status != QStringLiteral("refreshed")) ||
            !textField(object, QStringLiteral("device_id"), 9,
                       decoded.registration.deviceId) ||
            !containsDigits(decoded.registration.deviceId, 9) ||
            !textField(object, QStringLiteral("fingerprint"), 64,
                       decoded.registration.fingerprint) ||
            !isFingerprint(decoded.registration.fingerprint)) {
            error = QStringLiteral("Registration result fields are invalid.");
            return false;
        }
        error.clear();
        return true;
    }
    if (decoded.type == QStringLiteral("assist.code")) {
        decoded.kind = BootstrapMessage::Kind::AssistanceCode;
        QString protocol;
        qint64 lifetime = 0;
        if (!textField(object, QStringLiteral("protocol"), 64, protocol) ||
            protocol != QString::fromLatin1(kSessionProtocol) ||
            !textField(object, QStringLiteral("code"), 8,
                       decoded.assistance.code) ||
            !containsDigits(decoded.assistance.code, 8) ||
            !exactInteger(object.value(QStringLiteral("expires_in_seconds")),
                          1, 3600, lifetime)) {
            error = QStringLiteral("Assistance code fields are invalid.");
            return false;
        }
        decoded.assistance.expiresInSeconds = static_cast<int>(lifetime);
        error.clear();
        return true;
    }
    if (decoded.type == QStringLiteral("error")) {
        decoded.kind = BootstrapMessage::Kind::Error;
        if (!textField(object, QStringLiteral("code"), 64, decoded.error.code) ||
            !textField(object, QStringLiteral("message"), 512,
                       decoded.error.message)) {
            error = QStringLiteral("Signaling error fields are invalid.");
            return false;
        }
        error.clear();
        return true;
    }

    decoded.kind = BootstrapMessage::Kind::Other;
    error.clear();
    return true;
}

QByteArray encodeAssistanceRefresh() {
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("assist.refresh"));
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(kSessionProtocol));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace easydesk::client::signaling
