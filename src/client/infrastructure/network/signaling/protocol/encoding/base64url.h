#pragma once

#include <QByteArray>
#include <optional>

namespace easydesk::client::signaling {

QByteArray encodeBase64Url(const QByteArray &value);
std::optional<QByteArray> decodeBase64Url(const QByteArray &value);

}  // namespace easydesk::client::signaling
