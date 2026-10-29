#pragma once
#include "features/sessions/model/session_access.h"

#include <QString>
#include <QMetaType>

struct LocalIdentity {
    QString id, name, assistanceCode;
};
struct SessionOptions {
    int frameRate = 30;
    QString quality = QStringLiteral("清晰");
    SessionMode mode = SessionMode::ViewOnly;
};
struct SessionTelemetry {
    int latencyMs = 0, frameRate = 0;
    QString localIp, peerIp, transport, codec;
    double packetLossPercent = 0;
    bool peerToPeer = false;
};
Q_DECLARE_METATYPE(SessionTelemetry)
Q_DECLARE_METATYPE(SessionOptions)
