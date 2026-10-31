#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

inline constexpr auto kSignalingProtocol = "easydesk-signaling/1";

struct RegistrationChallenge {
    QString nonce;
    qint64 expiresAt = 0;
};

struct RegistrationFields {
    QString deviceId;
    QByteArray publicKey;
    QString displayName;
    QString platform;
    quint32 schemaVersion = 1;
    qint64 timestamp = 0;
    QString nonce;
};

bool parseRegistrationChallenge(const QByteArray &message,
                                RegistrationChallenge &challenge,
                                QString &error);
QByteArray registrationSigningBytes(const RegistrationFields &fields,
                                    QString &error);
QByteArray encodeRegistrationResponse(const RegistrationFields &fields,
                                      const QByteArray &signature,
                                      QString &error);

}  // namespace easydesk::client::signaling
