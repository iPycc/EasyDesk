#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace easydesk::client::signaling {

enum class SessionParticipantRole : quint8 {
    Controller = 1,
    Controlled = 2,
};

enum class SessionCapability : quint8 {
    ViewScreen = 1,
    SendInput = 2,
    FileReceive = 3,
};

struct SessionCapabilityGrant {
    QByteArray subjectPublicKey;
    SessionCapability capability = SessionCapability::ViewScreen;
};

bool canonicalizeSessionCapabilityGrants(
    QVector<SessionCapabilityGrant> &grants,
    const QByteArray &controllerPublicKey,
    const QByteArray &controlledPublicKey,
    QString &error);
bool sessionCapabilityGrantsAreSubset(
    const QVector<SessionCapabilityGrant> &granted,
    const QVector<SessionCapabilityGrant> &requested);

}  // namespace easydesk::client::signaling
