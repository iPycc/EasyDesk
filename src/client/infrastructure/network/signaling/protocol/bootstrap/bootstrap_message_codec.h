#pragma once

#include "network/signaling/protocol/registration/registration_codec.h"

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

inline constexpr auto kSessionProtocol = "easydesk-session/2";

struct RegistrationResultMessage {
    QString status;
    QString deviceId;
    QString fingerprint;
};

struct AssistanceCodeMessage {
    QString code;
    int expiresInSeconds = 0;
};

struct AssistanceResolvedMessage {
    QString deviceId;
    QByteArray publicKey;
    QString fingerprint;
};

struct SignalingErrorMessage {
    QString code;
    QString message;
};

struct BootstrapMessage {
    enum class Kind {
        RegistrationChallenge,
        RegistrationResult,
        AssistanceCode,
        AssistanceResolved,
        Error,
        Other,
    };

    Kind kind = Kind::Other;
    QString type;
    RegistrationChallenge challenge;
    RegistrationResultMessage registration;
    AssistanceCodeMessage assistance;
    AssistanceResolvedMessage resolved;
    SignalingErrorMessage error;
};

bool decodeBootstrapMessage(const QByteArray &message,
                            BootstrapMessage &decoded,
                            QString &error);
QByteArray encodeAssistanceRefresh();
QByteArray encodeAssistanceResolve(const QString &code, QString &error);

}  // namespace easydesk::client::signaling
