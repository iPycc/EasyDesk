#pragma once

#include "features/sessions/input/input_command.h"

#include <QByteArray>
#include <QString>

namespace easydesk::client::input {

// The stream is selected by the authenticated DataChannel, not by the payload.
bool encodeInputCommand(const InputCommand &command, QByteArray &output, QString &error);
bool decodeInputCommand(const QByteArray &data, InputStream stream,
                        InputCommand &command, QString &error);

}  // namespace easydesk::client::input
