#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

struct SessionDescriptionRoute {
    QByteArray requestId;
    QByteArray sessionId;
    quint32 sessionEpoch = 0;
    quint32 iceGeneration = 0;
    quint64 sequence = 0;
    qint64 expiresAt = 0;
    QString fromDeviceId;
    QString toDeviceId;
};

struct SessionOfferFields {
    SessionDescriptionRoute route;
    QByteArray signedAcceptPayload;
    QByteArray nonce;
    QString sdp;
};

struct SessionAnswerFields {
    SessionDescriptionRoute route;
    QByteArray signedOfferPayload;
    QString sdp;
};

QByteArray sessionOfferSigningBytes(const SessionOfferFields &fields,
                                    QString &error);
QByteArray encodeSessionOfferEnvelope(const SessionOfferFields &fields,
                                      const QByteArray &signature,
                                      QString &error);
QByteArray sessionAnswerSigningBytes(const SessionAnswerFields &fields,
                                     QString &error);
QByteArray encodeSessionAnswerEnvelope(const SessionAnswerFields &fields,
                                       const QByteArray &signature,
                                       QString &error);

}  // namespace easydesk::client::signaling
