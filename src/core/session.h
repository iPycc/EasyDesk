#pragma once
#include <QString>
#include <QMetaType>

struct LocalIdentity {
    QString id, name, assistanceCode;
};
struct SessionOptions {
    int frameRate = 30;
    QString quality = QStringLiteral("清晰");
    bool viewOnly = true;
};
struct SessionTelemetry {
    int latencyMs = 0, frameRate = 0;
    QString localIp, peerIp, transport;
    double packetLossPercent = 0;
    bool peerToPeer = false, simulated = true;
};
Q_DECLARE_METATYPE(SessionTelemetry)
Q_DECLARE_METATYPE(SessionOptions)
