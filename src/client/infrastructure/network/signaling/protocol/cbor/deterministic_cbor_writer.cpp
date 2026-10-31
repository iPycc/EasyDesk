#include "network/signaling/protocol/cbor/deterministic_cbor_writer.h"

#include <limits>
#include <utility>

namespace easydesk::client::signaling::cbor {
namespace {

constexpr quint8 kUnsignedMajorType = 0;
constexpr quint8 kBytesMajorType = 2;
constexpr quint8 kTextMajorType = 3;
constexpr quint8 kArrayMajorType = 4;

}  // namespace

void DeterministicCborWriter::beginArray(quint64 size) {
    writeHeader(kArrayMajorType, size);
}

void DeterministicCborWriter::writeUnsigned(quint64 value) {
    writeHeader(kUnsignedMajorType, value);
}

void DeterministicCborWriter::writeBytes(const QByteArray &value) {
    writeHeader(kBytesMajorType, static_cast<quint64>(value.size()));
    output_.append(value);
}

void DeterministicCborWriter::writeText(const QString &value) {
    const QByteArray utf8 = value.toUtf8();
    writeHeader(kTextMajorType, static_cast<quint64>(utf8.size()));
    output_.append(utf8);
}

const QByteArray &DeterministicCborWriter::bytes() const {
    return output_;
}

QByteArray DeterministicCborWriter::takeBytes() {
    return std::move(output_);
}

void DeterministicCborWriter::writeHeader(quint8 majorType, quint64 value) {
    const quint8 prefix = static_cast<quint8>(majorType << 5U);
    if (value < 24U) {
        output_.append(static_cast<char>(prefix | static_cast<quint8>(value)));
        return;
    }
    if (value <= std::numeric_limits<quint8>::max()) {
        output_.append(static_cast<char>(prefix | 24U));
        output_.append(static_cast<char>(value));
        return;
    }
    if (value <= std::numeric_limits<quint16>::max()) {
        output_.append(static_cast<char>(prefix | 25U));
        output_.append(static_cast<char>(value >> 8U));
        output_.append(static_cast<char>(value));
        return;
    }
    if (value <= std::numeric_limits<quint32>::max()) {
        output_.append(static_cast<char>(prefix | 26U));
        for (int shift = 24; shift >= 0; shift -= 8)
            output_.append(static_cast<char>(value >> static_cast<unsigned>(shift)));
        return;
    }
    output_.append(static_cast<char>(prefix | 27U));
    for (int shift = 56; shift >= 0; shift -= 8)
        output_.append(static_cast<char>(value >> static_cast<unsigned>(shift)));
}

}  // namespace easydesk::client::signaling::cbor
