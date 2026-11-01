#include "sessions/negotiation/turn/session_turn_binding.h"

#include "network/signaling/protocol/turn/turn_credentials_decoder.h"

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
