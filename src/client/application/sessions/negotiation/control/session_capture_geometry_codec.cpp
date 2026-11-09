#include "sessions/negotiation/control/session_capture_geometry_codec.h"

#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <cmath>

namespace {
constexpr int kMaximumMessageBytes = 1024;
constexpr int kMaximumDimension = 65535;

QString encoded(const QByteArray &value) {
    return QString::fromLatin1(easydesk::client::signaling::encodeBase64Url(value));
}

bool validGeometry(const RemoteCaptureGeometry &geometry) {
    if (geometry.generation == 0)
        return false;
    if (!geometry.valid)
        return geometry.size.width() == 0 && geometry.size.height() == 0;
    return geometry.size.width() > 0 && geometry.size.width() <= kMaximumDimension &&
           geometry.size.height() > 0 && geometry.size.height() <= kMaximumDimension;
}

QJsonObject binding(const SessionNegotiationContext &context) {
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("capture.geometry"));
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("request_id"), encoded(context.requestId));
    object.insert(QStringLiteral("session_id"), encoded(context.sessionId));
    object.insert(QStringLiteral("session_epoch"), QString::number(context.sessionEpoch));
    object.insert(QStringLiteral("permission_epoch"), QString::number(context.permissionEpoch));
    return object;
}

bool dimension(const QJsonValue &value, int &output) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < 0 || number > kMaximumDimension)
        return false;
    output = static_cast<int>(number);
    return true;
}
}  // namespace

QByteArray encodeCaptureGeometry(const SessionNegotiationContext &context,
                                 const RemoteCaptureGeometry &geometry, QString &error) {
    if (!context.isValid() || context.expiresAt <= QDateTime::currentSecsSinceEpoch() ||
        context.role != SessionRole::Controlled || !validGeometry(geometry)) {
        error = QStringLiteral("Capture geometry context is invalid.");
        return {};
    }
    QJsonObject object = binding(context);
    object.insert(QStringLiteral("generation"), QString::number(geometry.generation));
    object.insert(QStringLiteral("valid"), geometry.valid);
    object.insert(QStringLiteral("width"), geometry.size.width());
    object.insert(QStringLiteral("height"), geometry.size.height());
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool decodeCaptureGeometry(const QByteArray &message, const SessionNegotiationContext &context,
                           RemoteCaptureGeometry &geometry, QString &error) {
    geometry = {};
    if (!context.isValid() || context.expiresAt <= QDateTime::currentSecsSinceEpoch() ||
        context.role != SessionRole::Controller || message.isEmpty() ||
        message.size() > kMaximumMessageBytes) {
        error = QStringLiteral("Capture geometry context is invalid.");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Capture geometry message is invalid.");
        return false;
    }
    const QJsonObject object = document.object();
    const QJsonObject expected = binding(context);
    if (object.size() != 10) {
        error = QStringLiteral("Capture geometry fields are invalid.");
        return false;
    }
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        if (object.value(it.key()) != it.value()) {
            error = QStringLiteral("Capture geometry does not match the session.");
            return false;
        }
    }
    const QJsonValue generationValue = object.value(QStringLiteral("generation"));
    const QJsonValue validValue = object.value(QStringLiteral("valid"));
    bool generationOk = false;
    const QString generationText = generationValue.toString();
    const quint64 generation = generationText.toULongLong(&generationOk);
    int width = 0, height = 0;
    if (!generationValue.isString() || !generationOk ||
        generationText != QString::number(generation) || !validValue.isBool() ||
        !dimension(object.value(QStringLiteral("width")), width) ||
        !dimension(object.value(QStringLiteral("height")), height)) {
        error = QStringLiteral("Capture geometry fields are invalid.");
        return false;
    }
    const RemoteCaptureGeometry decoded{generation, QSize(width, height), validValue.toBool()};
    if (!validGeometry(decoded)) {
        error = QStringLiteral("Capture geometry dimensions are invalid.");
        return false;
    }
    geometry = decoded;
    error.clear();
    return true;
}
