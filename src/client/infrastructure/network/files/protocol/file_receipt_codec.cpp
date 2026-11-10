#include "network/files/protocol/file_receipt_codec.h"

#include <QBuffer>
#include <QDataStream>
#include <utility>

namespace easydesk::client::transfer {
namespace {
const QByteArray kMagic = QByteArrayLiteral("EDK2");
constexpr int kHeaderSize = 64;
bool token(const QByteArray &value) { return value.size() == 16 && value != QByteArray(16, '\0'); }
bool valid(const FileReceipt &receipt) {
    if (!token(receipt.identity.transferId) || !token(receipt.identity.fileId) ||
        !token(receipt.identity.generation)) return false;
    if (receipt.kind == ReceiptKind::Verified)
        return receipt.durability == Durability::OsCache && receipt.finalName.isEmpty();
    return receipt.kind == ReceiptKind::Committed && receipt.durability == Durability::OsFlushed &&
           isSafeWindowsFileName(receipt.finalName);
}
bool readBytes(QDataStream &stream, int length, QByteArray &value) {
    if (!stream.device() || length < 0 || stream.device()->bytesAvailable() < length) return false;
    value.resize(length);
    return stream.readRawData(value.data(), length) == length;
}
void writeBytes(QDataStream &stream, const QByteArray &value) {
    stream.writeRawData(value.constData(), value.size());
}
} // namespace

bool isFileReceipt(const QByteArray &message) { return message.startsWith(kMagic); }

bool encodeFileReceipt(const FileReceipt &receipt, QByteArray &message, QString &error) {
    message.clear();
    if (!valid(receipt)) { error = QStringLiteral("Invalid file receipt."); return false; }
    const QByteArray name = receipt.finalName.toUtf8();
    QDataStream stream(&message, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    writeBytes(stream, kMagic);
    writeBytes(stream, receipt.identity.transferId);
    writeBytes(stream, receipt.identity.fileId);
    writeBytes(stream, receipt.identity.generation);
    stream << receipt.verifiedOffset << static_cast<quint8>(receipt.kind)
           << static_cast<quint8>(receipt.durability) << static_cast<quint16>(name.size());
    writeBytes(stream, name);
    if (stream.status() != QDataStream::Ok || message.size() != kHeaderSize + name.size()) {
        message.clear(); error = QStringLiteral("File receipt could not be encoded."); return false;
    }
    error.clear();
    return true;
}

bool decodeFileReceipt(const QByteArray &message, FileReceipt &receipt, QString &error) {
    receipt = {};
    auto fail = [&] { error = QStringLiteral("Invalid file receipt."); return false; };
    if (!isFileReceipt(message) || message.size() < kHeaderSize || message.size() > kHeaderSize + 1020)
        return fail();
    QBuffer buffer;
    buffer.setData(message);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    FileReceipt decoded;
    QByteArray magic, name;
    quint8 kind = 0, durability = 0;
    quint16 length = 0;
    if (!readBytes(stream, 4, magic) || !readBytes(stream, 16, decoded.identity.transferId) ||
        !readBytes(stream, 16, decoded.identity.fileId) || !readBytes(stream, 16, decoded.identity.generation))
        return fail();
    stream >> decoded.verifiedOffset >> kind >> durability >> length;
    if (stream.status() != QDataStream::Ok || length > 1020 || !readBytes(stream, length, name) || !buffer.atEnd())
        return fail();
    decoded.finalName = QString::fromUtf8(name.constData(), name.size());
    decoded.kind = static_cast<ReceiptKind>(kind);
    decoded.durability = static_cast<Durability>(durability);
    if (decoded.finalName.toUtf8() != name || !valid(decoded)) return fail();
    receipt = std::move(decoded);
    error.clear();
    return true;
}
} // namespace easydesk::client::transfer
