#pragma once

#include "network/signaling/protocol/session/ice/session_ice_codec.h"

namespace easydesk::client::signaling {

struct SessionIceContext {
    QByteArray requestId;
    QByteArray sessionId;
    quint32 sessionEpoch = 0;
    quint32 iceGeneration = 0;
    quint64 descriptionSequence = 0;
    QString fromDeviceId;
    QString toDeviceId;
    QByteArray signedDescriptionPayload;
};

struct DecodedSessionCandidates {
    SessionCandidatesFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

struct DecodedSessionGatheringComplete {
    SessionGatheringCompleteFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionCandidatesEnvelope(const QByteArray &message,
                                     const SessionIceContext &context,
                                     DecodedSessionCandidates &candidates,
                                     QString &error);
bool decodeSessionGatheringCompleteEnvelope(
    const QByteArray &message,
    const SessionIceContext &context,
    DecodedSessionGatheringComplete &complete,
    QString &error);

}  // namespace easydesk::client::signaling
