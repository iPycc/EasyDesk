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

struct SignalingErrorMessage {
    QString code;
    QString message;
};

struct BootstrapMessage {
    enum class Kind {
        RegistrationChallenge,
        RegistrationResult,
        AssistanceCode,
        Error,
        Other,
    };

    Kind kind = Kind::Other;
    QString type;
    RegistrationChallenge challenge;
    RegistrationResultMessage registration;
    AssistanceCodeMessage assistance;
    SignalingErrorMessage error;
};

bool decodeBootstrapMessage(const QByteArray &message,
                            BootstrapMessage &decoded,
                            QString &error);
QByteArray encodeAssistanceRefresh();

}  // namespace easydesk::client::signaling
