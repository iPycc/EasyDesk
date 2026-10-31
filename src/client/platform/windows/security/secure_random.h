#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::windows {

QByteArray generateSecureRandomBytes(int size, QString &error);

}  // namespace easydesk::client::windows
