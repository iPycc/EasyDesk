#pragma once

#include "features/sessions/input/input_command.h"

#include <QByteArray>
#include <QString>

namespace easydesk::client::input {

enum class InputLeaseMessageKind : quint8 { Challenge = 1, Renewal = 2 };

struct InputLeaseMessage {
    InputLeaseMessageKind kind = InputLeaseMessageKind::Challenge;
    InputCommandContext context;
    quint64 leaseGeneration = 0;
    QByteArray challenge;
    quint32 timeoutMilliseconds = 0;
};

bool encodeInputLeaseMessage(const InputLeaseMessage &message, QByteArray &output, QString &error);
bool decodeInputLeaseMessage(const QByteArray &data, InputLeaseMessage &message, QString &error);
bool isInputLeaseMessage(const QByteArray &data);

}  // namespace easydesk::client::input
