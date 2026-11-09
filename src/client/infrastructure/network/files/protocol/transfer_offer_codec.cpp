#include "network/files/protocol/transfer_offer_codec.h"

#include <QBuffer>
#include <QDataStream>
#include <utility>

namespace easydesk::client::transfer {
namespace {
constexpr int kHeaderSize = 60;
constexpr int kFrameSize = 4096;
constexpr int kPayloadPerFrame = kFrameSize - kHeaderSize;
constexpr quint32 kMaximumPayload = 128 * 1024;
const QByteArray kMagic = QByteArrayLiteral("EDF1");

bool readExact(QDataStream &stream, int size, QByteArray &output) {
    if (size < 0 || !stream.device() || stream.device()->bytesAvailable() < size)
        return false;
    output.resize(size);
    return stream.readRawData(output.data(), size) == size;
}

QByteArray encodePayload(const TransferManifest &manifest, quint64 total) {
    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << manifest.version << static_cast<quint16>(manifest.files.size()) << total;
    for (const ManifestFile &file : manifest.files) {
        const auto name = file.name.toUtf8();
        stream.writeRawData(file.fileId.constData(), file.fileId.size());
        stream.writeRawData(file.sourceGeneration.constData(), file.sourceGeneration.size());
        stream << static_cast<quint16>(name.size());
        stream.writeRawData(name.constData(), name.size());
        stream << file.size;
        stream.writeRawData(file.sha256.constData(), file.sha256.size());
    }
    return stream.status() == QDataStream::Ok ? output : QByteArray();
}

bool decodePayload(const QByteArray &data, const QByteArray &id, quint64 maximumFileBytes,
                   TransferManifest &manifest, QString &error) {
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    TransferManifest decoded;
    decoded.transferId = id;
    quint16 count = 0;
    quint64 declaredTotal = 0;
    stream >> decoded.version >> count >> declaredTotal;
    if (stream.status() != QDataStream::Ok || count == 0 || count > kMaximumFiles)
        return false;
    decoded.files.reserve(count);
    for (quint16 i = 0; i < count; ++i) {
        ManifestFile file;
        QByteArray name;
        quint16 nameLength = 0;
        if (!readExact(stream, 16, file.fileId) || !readExact(stream, 16, file.sourceGeneration))
            return false;
        stream >> nameLength;
        if (stream.status() != QDataStream::Ok || nameLength == 0 || nameLength > 1020 ||
            !readExact(stream, nameLength, name))
            return false;
        file.name = QString::fromUtf8(name.constData(), name.size());
        if (file.name.toUtf8() != name)
            return false;
        stream >> file.size;
        if (stream.status() != QDataStream::Ok || !readExact(stream, 32, file.sha256))
            return false;
        decoded.files.append(std::move(file));
    }
    quint64 actualTotal = 0;
    if (!buffer.atEnd() || !validateManifest(decoded, maximumFileBytes, actualTotal, error) ||
        actualTotal != declaredTotal)
        return false;
    manifest = std::move(decoded);
    return true;
}
} // namespace

bool encodeTransferOffer(const TransferManifest &manifest, QVector<QByteArray> &frames,
                         QString &error) {
    frames.clear();
    quint64 total = 0;
    if (!validateManifest(manifest, kDefaultMaximumFileBytes, total, error))
        return false;
    const QByteArray digest = manifestDigest(manifest, kDefaultMaximumFileBytes, error);
    const QByteArray payload = encodePayload(manifest, total);
    if (digest.size() != 32 || payload.isEmpty() || payload.size() > static_cast<int>(kMaximumPayload)) {
        error = QStringLiteral("Transfer offer is too large or could not be encoded.");
        return false;
    }
    const quint16 count = static_cast<quint16>((payload.size() + kPayloadPerFrame - 1) / kPayloadPerFrame);
    frames.reserve(count);
    for (quint16 index = 0; index < count; ++index) {
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.writeRawData(kMagic.constData(), kMagic.size());
        stream.writeRawData(manifest.transferId.constData(), manifest.transferId.size());
        stream.writeRawData(digest.constData(), digest.size());
        stream << static_cast<quint32>(payload.size()) << count << index;
        const QByteArray part = payload.mid(index * kPayloadPerFrame, kPayloadPerFrame);
        stream.writeRawData(part.constData(), part.size());
        if (stream.status() != QDataStream::Ok) {
            frames.clear();
            error = QStringLiteral("Transfer offer could not be encoded.");
            return false;
        }
        frames.append(std::move(frame));
    }
    error.clear();
    return true;
}

TransferOfferDecoder::TransferOfferDecoder(quint64 maximumFileBytes)
    : m_maximumFileBytes(maximumFileBytes) {}

void TransferOfferDecoder::reset() {
    m_transferId.clear();
    m_digest.clear();
    m_payload.clear();
    m_payloadSize = 0;
    m_count = 0;
    m_next = 0;
}

TransferOfferDecoder::Result TransferOfferDecoder::receive(const QByteArray &frame,
                                                            TransferManifest &manifest,
                                                            QString &error) {
    manifest = {};
    auto invalid = [&] {
        reset();
        error = QStringLiteral("Invalid transfer offer.");
        return Result::Invalid;
    };
    if (frame.size() <= kHeaderSize || frame.size() > kFrameSize)
        return invalid();
    QBuffer buffer;
    buffer.setData(frame);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    QByteArray magic, id, digest;
    quint32 size = 0;
    quint16 count = 0, index = 0;
    if (!readExact(stream, 4, magic) || !readExact(stream, 16, id) ||
        !readExact(stream, 32, digest))
        return invalid();
    stream >> size >> count >> index;
    if (stream.status() != QDataStream::Ok || magic != kMagic || size == 0 || size > kMaximumPayload ||
        count != (size + kPayloadPerFrame - 1) / kPayloadPerFrame || index >= count)
        return invalid();
    const int expectedPartSize = index + 1 == count
                                     ? static_cast<int>(size) - index * kPayloadPerFrame
                                     : kPayloadPerFrame;
    if (buffer.bytesAvailable() != expectedPartSize)
        return invalid();
    if (m_next == 0) {
        if (index != 0)
            return invalid();
        m_transferId = id;
        m_digest = digest;
        m_payloadSize = size;
        m_count = count;
        m_payload.reserve(static_cast<int>(size));
    }
    if (id != m_transferId || digest != m_digest || size != m_payloadSize || count != m_count ||
        index != m_next)
        return invalid();
    m_payload.append(buffer.readAll());
    ++m_next;
    if (m_next < m_count) {
        error.clear();
        return Result::Pending;
    }
    TransferManifest decoded;
    if (m_payload.size() != static_cast<int>(m_payloadSize) ||
        !decodePayload(m_payload, m_transferId, m_maximumFileBytes, decoded, error) ||
        manifestDigest(decoded, m_maximumFileBytes, error) != m_digest)
        return invalid();
    manifest = std::move(decoded);
    reset();
    error.clear();
    return Result::Complete;
}

} // namespace easydesk::client::transfer
