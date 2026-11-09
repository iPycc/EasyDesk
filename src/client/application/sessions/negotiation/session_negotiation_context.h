#pragma once

#include "features/sessions/model/session_state.h"

#include <QByteArray>
#include <QString>

struct SessionNegotiationContext {
    QByteArray requestId;
    QByteArray sessionId;
    QByteArray signedRequestPayload;
    QByteArray signedAcceptPayload;
    QByteArray acceptanceNonce;
    QByteArray peerPublicKey;
    QString localDeviceId;
    QString peerDeviceId;
    quint32 sessionEpoch = 0;
    quint64 permissionEpoch = 0;
    qint64 expiresAt = 0;
    SessionRole role = SessionRole::Controller;
    SessionCapabilities capabilities;
    bool localMayReceiveFiles = false;
    bool peerMayReceiveFiles = false;
    bool offerer = false;

    bool isValid() const;
};
