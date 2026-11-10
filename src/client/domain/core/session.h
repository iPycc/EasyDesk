#pragma once
#include "features/sessions/model/session_access.h"
#include "features/sessions/input/input_command.h"

#include <QString>
#include <QMetaType>
#include <QSize>

struct LocalIdentity {
    QString id, name, assistanceCode;
    QString fingerprint;
};
struct SessionOptions {
    int frameRate = 30;
    QString quality = QStringLiteral("清晰");
    SessionMode mode = SessionMode::ViewOnly;
    bool requestPeerFileReceive = false;
};
struct SessionTelemetry {
    int latencyMs = 0, frameRate = 0;
    QString localIp, peerIp, transport, codec;
    double packetLossPercent = 0;
    bool peerToPeer = false;
};
struct IncomingSessionRequest {
    QString fromDeviceId;
    SessionMode requestedMode = SessionMode::ViewOnly;
    QString localFingerprint;
    QString peerFingerprint;
    bool canRememberIdentity = false;
    bool identityVerified = false;
    bool requestsLocalFileReceive = false;
};
struct PeerIdentityPrompt {
    QString deviceId;
    QString localFingerprint;
    QString peerFingerprint;
    bool canRemember = false;
};
struct SessionInputStatus {
    bool ready = false;
    QSize presentedFrame;
    QSize presentedTarget;
    QSize remoteRegion;
    quint64 displayGeneration = 0;

    bool operator==(const SessionInputStatus &other) const {
        return ready == other.ready && presentedFrame == other.presentedFrame && presentedTarget == other.presentedTarget &&
               remoteRegion == other.remoteRegion && displayGeneration == other.displayGeneration;
    }
    bool operator!=(const SessionInputStatus &other) const { return !(*this == other); }
};
Q_DECLARE_METATYPE(SessionTelemetry)
Q_DECLARE_METATYPE(SessionOptions)
Q_DECLARE_METATYPE(IncomingSessionRequest)
Q_DECLARE_METATYPE(PeerIdentityPrompt)
Q_DECLARE_METATYPE(SessionInputStatus)
