#pragma once

#include "network/signaling/protocol/session/description/session_description_codec.h"

#include <QVector>

namespace easydesk::client::signaling {

struct SessionIceCandidate {
    QString sdpMid;
    quint32 sdpMLineIndex = 0;
    QString usernameFragment;
    QString candidate;
};

struct SessionCandidatesFields {
    SessionDescriptionRoute route;
    QByteArray signedDescriptionPayload;
    QVector<SessionIceCandidate> candidates;
};

struct SessionGatheringCompleteFields {
    SessionDescriptionRoute route;
    QByteArray signedDescriptionPayload;
};

QByteArray sessionCandidatesSigningBytes(const SessionCandidatesFields &fields,
                                         QString &error);
QByteArray encodeSessionCandidatesEnvelope(const SessionCandidatesFields &fields,
                                           const QByteArray &signature,
                                           QString &error);
QByteArray sessionGatheringCompleteSigningBytes(
    const SessionGatheringCompleteFields &fields,
    QString &error);
QByteArray encodeSessionGatheringCompleteEnvelope(
    const SessionGatheringCompleteFields &fields,
    const QByteArray &signature,
    QString &error);

}  // namespace easydesk::client::signaling
