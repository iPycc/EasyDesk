#include "sessions/negotiation/turn/session_turn_binding.h"

#include "network/signaling/protocol/turn/turn_credentials_decoder.h"
#include "network/signaling/protocol/encoding/base64url.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

constexpr qint64 kMaximumCredentialLifetimeSeconds = 30 * 60;
constexpr qint64 kClockSkewAllowanceSeconds = 30;

bool usernameMatchesDevice(const QString &username, const QString &deviceId) {
    const QStringList parts = username.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    return parts.size() == 3 && parts.at(1) == deviceId;
}

}  // namespace

bool bindSessionTurnCredentials(const QByteArray &message,
                                const SessionNegotiationContext &context,
                                qint64 now,
                                SessionTurnConfiguration &configuration,
                                QString &error) {
    configuration = {};
    if (!context.isValid() || now <= 0) {
        error = QStringLiteral("TURN session context is invalid.");
        return false;
    }

    easydesk::client::signaling::TurnCredentialsMessage credentials;
    if (!easydesk::client::signaling::decodeTurnCredentials(
            message, credentials, error)) {
        return false;
    }
    if (credentials.requestId != context.requestId ||
        credentials.sessionId != context.sessionId ||
        credentials.sessionEpoch != context.sessionEpoch ||
        !usernameMatchesDevice(credentials.username, context.localDeviceId)) {
        error = QStringLiteral("TURN credentials do not match the active session.");
        return false;
    }
    if (credentials.expiresAt <= now ||
        credentials.expiresAt >
            now + kMaximumCredentialLifetimeSeconds + kClockSkewAllowanceSeconds ||
        credentials.expiresAt > context.expiresAt) {
        error = QStringLiteral("TURN credential lifetime is invalid.");
        return false;
    }

    for (const QString &url : credentials.urls) {
        for (const SessionTurnServer &server : configuration.servers) {
            if (server.url == url) {
                error = QStringLiteral("TURN credential URLs contain duplicates.");
                configuration = {};
                return false;
            }
        }
        configuration.servers.append(
            {url, credentials.username, credentials.password});
    }
    configuration.expiresAt = credentials.expiresAt;
    error.clear();
    return true;
}

bool bindSessionTurnUnavailable(const QByteArray &message,
                                const SessionNegotiationContext &context,
                                QString &error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (!context.isValid() || parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        error = QStringLiteral("TURN availability message is invalid.");
        return false;
    }
    const QJsonObject object = document.object();
    const QJsonValue epoch = object.value(QStringLiteral("session_epoch"));
    const QByteArray request = easydesk::client::signaling::encodeBase64Url(context.requestId);
    const QByteArray session = easydesk::client::signaling::encodeBase64Url(context.sessionId);
    if (object.size() != 5 ||
        object.value(QStringLiteral("type")) != QStringLiteral("turn.unavailable") ||
        object.value(QStringLiteral("protocol")) != QStringLiteral("easydesk-session/2") ||
        object.value(QStringLiteral("request_id")) != QString::fromLatin1(request) ||
        object.value(QStringLiteral("session_id")) != QString::fromLatin1(session) ||
        !epoch.isDouble() || epoch.toDouble() != static_cast<double>(context.sessionEpoch)) {
        error = QStringLiteral("TURN availability does not match the active session.");
        return false;
    }
    error.clear();
    return true;
}
