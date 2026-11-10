#include "network/files/protocol/file_data_codec.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <type_traits>

namespace easydesk::client::transfer {
namespace {
const QByteArray kBegin = QByteArrayLiteral("EDB2");
const QByteArray kChunk = QByteArrayLiteral("EDC2");
const QByteArray kCommit = QByteArrayLiteral("EDE2");
constexpr int kHeaderSize = 4 + 16 * 3;
constexpr int kMaximumFrame = 16384;

bool token(const QByteArray &value) { return value.size() == 16 && value != QByteArray(16, '\0'); }
bool validIdentity(const FileMessageIdentity &identity) {
    return token(identity.transferId) && token(identity.fileId) && token(identity.generation);
}
bool readBytes(QDataStream &stream, int count, QByteArray &out) {
    if (!stream.device() || count < 0 || stream.device()->bytesAvailable() < count)
        return false;
    out.resize(count);
    return stream.readRawData(out.data(), count) == count;
}
void writeBytes(QDataStream &stream, const QByteArray &data) {
    stream.writeRawData(data.constData(), data.size());
}
bool valid(const FileDataMessage &message) {
    return std::visit([](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if (!validIdentity(value.identity))
            return false;
        if constexpr (std::is_same_v<T, FileBegin>)
            return value.chunkSize > 0 && value.chunkSize <= kMaximumChunkPayload;
        if constexpr (std::is_same_v<T, FileChunk>)
            return !value.bytes.isEmpty() && value.bytes.size() <= kMaximumChunkPayload &&
                   value.sha256.size() == 32 &&
                   QCryptographicHash::hash(value.bytes, QCryptographicHash::Sha256) == value.sha256;
        if constexpr (std::is_same_v<T, FileCommit>)
            return value.sha256.size() == 32;
        return false;
    }, message);
}
} // namespace

bool encodeFileDataMessage(const FileDataMessage &message, QByteArray &output, QString &error) {
    output.clear();
    if (!valid(message)) {
        error = QStringLiteral("Invalid file data message.");
        return false;
    }
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    std::visit([&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, FileBegin>) writeBytes(stream, kBegin);
        if constexpr (std::is_same_v<T, FileChunk>) writeBytes(stream, kChunk);
        if constexpr (std::is_same_v<T, FileCommit>) writeBytes(stream, kCommit);
        writeBytes(stream, value.identity.transferId);
        writeBytes(stream, value.identity.fileId);
        writeBytes(stream, value.identity.generation);
        if constexpr (std::is_same_v<T, FileBegin>) stream << value.size << value.chunkSize;
        if constexpr (std::is_same_v<T, FileChunk>) {
            stream << value.offset;
            writeBytes(stream, value.sha256);
            writeBytes(stream, value.bytes);
        }
        if constexpr (std::is_same_v<T, FileCommit>) {
            stream << value.size;
            writeBytes(stream, value.sha256);
        }
    }, message);
    if (stream.status() != QDataStream::Ok || output.size() > kMaximumFrame) {
        output.clear();
        error = QStringLiteral("File data message is too large.");
        return false;
    }
    error.clear();
    return true;
}

bool decodeFileDataMessage(const QByteArray &input, FileDataMessage &message, QString &error) {
    auto fail = [&] { error = QStringLiteral("Invalid file data message."); return false; };
    if (input.size() <= kHeaderSize || input.size() > kMaximumFrame)
        return fail();
    QBuffer buffer;
    buffer.setData(input);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    QByteArray magic;
    FileMessageIdentity identity;
    if (!readBytes(stream, 4, magic) || !readBytes(stream, 16, identity.transferId) ||
        !readBytes(stream, 16, identity.fileId) || !readBytes(stream, 16, identity.generation))
        return fail();
    FileDataMessage parsed;
    if (magic == kBegin) {
        FileBegin value;
        value.identity = identity;
        stream >> value.size >> value.chunkSize;
        parsed = value;
    } else if (magic == kChunk) {
        FileChunk value;
        value.identity = identity;
        stream >> value.offset;
        if (!readBytes(stream, 32, value.sha256)) return fail();
        value.bytes = buffer.readAll();
        parsed = value;
    } else if (magic == kCommit) {
        FileCommit value;
        value.identity = identity;
        stream >> value.size;
        if (!readBytes(stream, 32, value.sha256)) return fail();
        parsed = value;
    } else {
        return fail();
    }
    if (stream.status() != QDataStream::Ok || !buffer.atEnd() || !valid(parsed))
        return fail();
    message = std::move(parsed);
    error.clear();
    return true;
}
} // namespace easydesk::client::transfer
