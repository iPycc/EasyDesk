#pragma once

#include "network/signaling/protocol/session/description/session_description_codec.h"

namespace easydesk::client::signaling {

struct SessionDescriptionContext {
    QByteArray requestId;
    QByteArray sessionId;
    quint32 sessionEpoch = 0;
    QString offererDeviceId;
    QString answererDeviceId;
    QByteArray signedAcceptPayload;
};

struct DecodedSessionOffer {
    SessionOfferFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

struct DecodedSessionAnswer {
    SessionAnswerFields fields;
    QByteArray signedPayload;
    QByteArray signature;
};

bool decodeSessionOfferEnvelope(const QByteArray &message,
                                const SessionDescriptionContext &context,
                                DecodedSessionOffer &offer,
                                QString &error);
bool decodeSessionAnswerEnvelope(const QByteArray &message,
                                 const SessionDescriptionContext &context,
                                 const QByteArray &signedOfferPayload,
                                 DecodedSessionAnswer &answer,
                                 QString &error);

}  // namespace easydesk::client::signaling
