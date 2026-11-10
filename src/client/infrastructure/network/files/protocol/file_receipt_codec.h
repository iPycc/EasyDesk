#pragma once

#include "network/files/protocol/file_data_codec.h"

namespace easydesk::client::transfer {

enum class ReceiptKind : quint8 { Verified = 1, Committed = 2 };
// OsFlushed means file contents were flushed; it does not promise crash-durable directory metadata.
enum class Durability : quint8 { OsCache = 1, OsFlushed = 2 };
struct FileReceipt {
    FileMessageIdentity identity;
    quint64 verifiedOffset = 0;
    ReceiptKind kind = ReceiptKind::Verified;
    Durability durability = Durability::OsCache;
    QString finalName;
};

bool isFileReceipt(const QByteArray &message);
bool encodeFileReceipt(const FileReceipt &receipt, QByteArray &message, QString &error);
bool decodeFileReceipt(const QByteArray &message, FileReceipt &receipt, QString &error);
} // namespace easydesk::client::transfer
