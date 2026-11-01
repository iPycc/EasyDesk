#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling {

bool extractSha256DtlsFingerprint(const QString &sdp,
                                  QByteArray &fingerprint,
                                  QString &error);

}  // namespace easydesk::client::signaling
