#include "sessions/negotiation/control/session_ready_exchange.h"

#include "network/signaling/protocol/encoding/base64url.h"
#include "security/secure_random.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
using easydesk::client::signaling::decodeBase64Url;
using easydesk::client::signaling::encodeBase64Url;

QString encoded(const QByteArray &value) {
    return QString::fromLatin1(encodeBase64Url(value));
}

QJsonObject commonFields(const SessionNegotiationContext &context, const char *type) {
    QJsonObject object;
    object.insert(QStringLiteral("type"), QString::fromLatin1(type));
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("request_id"), encoded(context.requestId));
    object.insert(QStringLiteral("session_id"), encoded(context.sessionId));
    object.insert(QStringLiteral("session_epoch"), QString::number(context.sessionEpoch));
    object.insert(QStringLiteral("permission_epoch"), QString::number(context.permissionEpoch));
    return object;
}

bool parseCommon(const QByteArray &message, const SessionNegotiationContext &context,
                 const char *type, int expectedSize, QJsonObject &object) {
    if (message.isEmpty() || message.size() > 1024 || !context.isValid() ||
        QDateTime::currentSecsSinceEpoch() >= context.expiresAt)
        return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(message, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    object = document.object();
    const auto expected = commonFields(context, type);
    if (object.size() != expectedSize)
        return false;
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        if (object.value(it.key()) != it.value())
            return false;
    }
    return true;
}

QString roleName(SessionRole role) {
    return role == SessionRole::Controller ? QStringLiteral("controller")
                                           : QStringLiteral("controlled");
}

QString authorizationHash(const SessionNegotiationContext &context) {
    return encoded(QCryptographicHash::hash(context.signedAcceptPayload,
                                             QCryptographicHash::Sha256));
}
}  // namespace

bool SessionReadyExchange::start(const SessionNegotiationContext &context, QString &error) {
    if (!m_localChallenge.isEmpty() || !context.isValid() ||
        context.expiresAt <= QDateTime::currentSecsSinceEpoch()) {
        error = QStringLiteral("Session readiness context is invalid.");
        return false;
    }
    const QByteArray challenge = easydesk::client::windows::generateSecureRandomBytes(32, error);
    if (challenge.size() != 32)
        return false;
    m_context = context;
    m_localChallenge = challenge;
    error.clear();
    return true;
}

void SessionReadyExchange::stop() {
    m_context = {};
    m_localChallenge.clear();
    m_readySent = false;
    m_peerReady = false;
    m_acknowledged = false;
}

QByteArray SessionReadyExchange::makeReady(QString &error) {
    if (m_localChallenge.size() != 32 || m_readySent ||
        QDateTime::currentSecsSinceEpoch() >= m_context.expiresAt) {
        error = QStringLiteral("Session readiness message cannot be sent.");
        return {};
    }
    QJsonObject object = commonFields(m_context, "session.ready");
    object.insert(QStringLiteral("role"), roleName(m_context.role));
    object.insert(QStringLiteral("authorization_hash"), authorizationHash(m_context));
    object.insert(QStringLiteral("challenge"), encoded(m_localChallenge));
    m_readySent = true;
    error.clear();
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SessionReadyExchange::receiveReady(const QByteArray &message,
                                        QByteArray &acknowledgement, QString &error) {
    acknowledgement.clear();
    QJsonObject object;
    if (m_localChallenge.size() != 32 || m_peerReady ||
        !parseCommon(message, m_context, "session.ready", 9, object) ||
        object.value(QStringLiteral("role")) !=
            roleName(m_context.role == SessionRole::Controller ? SessionRole::Controlled
                                                               : SessionRole::Controller) ||
        object.value(QStringLiteral("authorization_hash")) != authorizationHash(m_context) ||
        !object.value(QStringLiteral("challenge")).isString()) {
        error = QStringLiteral("Peer session readiness message is invalid.");
        return false;
    }
    const QString challengeText = object.value(QStringLiteral("challenge")).toString();
    const auto challenge = decodeBase64Url(challengeText.toLatin1());
    if (!challenge || challenge->size() != 32 || encoded(*challenge) != challengeText) {
        error = QStringLiteral("Peer session readiness challenge is invalid.");
        return false;
    }
    QJsonObject response = commonFields(m_context, "session.ready_ack");
    response.insert(QStringLiteral("challenge"), challengeText);
    acknowledgement = QJsonDocument(response).toJson(QJsonDocument::Compact);
    m_peerReady = true;
    error.clear();
    return true;
}

bool SessionReadyExchange::receiveAcknowledgement(const QByteArray &message, QString &error) {
    QJsonObject object;
    if (!m_readySent || m_acknowledged ||
        !parseCommon(message, m_context, "session.ready_ack", 7, object) ||
        object.value(QStringLiteral("challenge")) != encoded(m_localChallenge)) {
        error = QStringLiteral("Session readiness acknowledgement is invalid.");
        return false;
    }
    m_acknowledged = true;
    error.clear();
    return true;
}

bool SessionReadyExchange::isReady() const {
    return m_readySent && m_peerReady && m_acknowledged &&
           QDateTime::currentSecsSinceEpoch() < m_context.expiresAt;
}
