#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::input {

struct InputEnableRequest {
    QByteArray sessionId;
    quint64 sessionEpoch = 0;
    quint64 permissionEpoch = 0;
    quint64 displayGeneration = 0;
    quint64 sequence = 0;
};

bool isInputEnableRequest(const QByteArray &data);
bool encodeInputEnableRequest(const InputEnableRequest &request, QByteArray &output, QString &error);
bool decodeInputEnableRequest(const QByteArray &data, InputEnableRequest &request, QString &error);

}  // namespace easydesk::client::input
