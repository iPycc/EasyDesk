#pragma once

#include <QByteArray>
#include <QString>
#include <variant>

namespace easydesk::client::transfer {

constexpr int kMaximumChunkPayload = 16292;

struct FileMessageIdentity {
    QByteArray transferId;
    QByteArray fileId;
    QByteArray generation;
};
struct FileBegin {
    FileMessageIdentity identity;
    quint64 size = 0;
    quint16 chunkSize = 0;
};
struct FileChunk {
    FileMessageIdentity identity;
    quint64 offset = 0;
    QByteArray sha256;
    QByteArray bytes;
};
struct FileCommit {
    FileMessageIdentity identity;
    quint64 size = 0;
    QByteArray sha256;
};
using FileDataMessage = std::variant<FileBegin, FileChunk, FileCommit>;

bool encodeFileDataMessage(const FileDataMessage &message, QByteArray &output, QString &error);
bool decodeFileDataMessage(const QByteArray &input, FileDataMessage &message, QString &error);

} // namespace easydesk::client::transfer
