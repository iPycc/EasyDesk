#pragma once

#include "sessions/negotiation/session_negotiation_context.h"

#include <QSize>

struct RemoteCaptureGeometry {
    quint64 generation = 0;
    QSize size;
    bool valid = false;
};

QByteArray encodeCaptureGeometry(const SessionNegotiationContext &context,
                                 const RemoteCaptureGeometry &geometry, QString &error);
bool decodeCaptureGeometry(const QByteArray &message, const SessionNegotiationContext &context,
                           RemoteCaptureGeometry &geometry, QString &error);
