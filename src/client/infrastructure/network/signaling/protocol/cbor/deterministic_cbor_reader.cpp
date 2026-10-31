#include "network/signaling/protocol/cbor/deterministic_cbor_reader.h"

#include <limits>

namespace easydesk::client::signaling::cbor {
namespace {

constexpr quint8 kUnsignedMajorType = 0;
constexpr quint8 kBytesMajorType = 2;
constexpr quint8 kTextMajorType = 3;
constexpr quint8 kArrayMajorType = 4;

bool isContinuation(quint8 value) {
    return value >= 0x80U && value <= 0xbfU;
}

bool isValidUtf8(const QByteArray &value) {
    int index = 0;
    while (index < value.size()) {
        const quint8 first = static_cast<quint8>(value.at(index++));
        if (first <= 0x7fU)
            continue;
        if (first >= 0xc2U && first <= 0xdfU) {
            if (index >= value.size() ||
                !isContinuation(static_cast<quint8>(value.at(index++)))) {
                return false;
            }
            continue;
        }
        if (first >= 0xe0U && first <= 0xefU) {
            if (value.size() - index < 2)
                return false;
            const quint8 second = static_cast<quint8>(value.at(index++));
            const quint8 third = static_cast<quint8>(value.at(index++));
            if (!isContinuation(third) ||
                (first == 0xe0U ? second < 0xa0U || second > 0xbfU
                                : first == 0xedU ? second < 0x80U || second > 0x9fU
                                                 : !isContinuation(second))) {
                return false;
            }
            continue;
        }
        if (first >= 0xf0U && first <= 0xf4U) {
            if (value.size() - index < 3)
                return false;
            const quint8 second = static_cast<quint8>(value.at(index++));
            const quint8 third = static_cast<quint8>(value.at(index++));
            const quint8 fourth = static_cast<quint8>(value.at(index++));
            if (!isContinuation(third) || !isContinuation(fourth) ||
                (first == 0xf0U ? second < 0x90U || second > 0xbfU
                                : first == 0xf4U ? second < 0x80U || second > 0x8fU
                                                 : !isContinuation(second))) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

DeterministicCborReader::DeterministicCborReader(const QByteArray &input)
    : m_input(input) {}

std::optional<quint64> DeterministicCborReader::readArraySize() {
    return readHeader(kArrayMajorType);
}

std::optional<quint64> DeterministicCborReader::readUnsigned() {
    return readHeader(kUnsignedMajorType);
}

std::optional<QByteArray> DeterministicCborReader::readBytes(int maximumBytes) {
    return readValue(kBytesMajorType, maximumBytes);
}

std::optional<QString> DeterministicCborReader::readText(int maximumBytes) {
    const auto value = readValue(kTextMajorType, maximumBytes);
    if (!value || !isValidUtf8(*value))
        return std::nullopt;
    return QString::fromUtf8(*value);
}

bool DeterministicCborReader::atEnd() const {
    return m_offset == m_input.size();
}

std::optional<quint64> DeterministicCborReader::readHeader(quint8 expectedMajorType) {
    if (m_offset >= m_input.size())
        return std::nullopt;
    const quint8 initial = static_cast<quint8>(m_input.at(m_offset++));
    if ((initial >> 5U) != expectedMajorType)
        return std::nullopt;

    const quint8 additional = initial & 0x1fU;
    if (additional < 24U)
        return additional;

    int byteCount = 0;
    if (additional == 24U)
        byteCount = 1;
    else if (additional == 25U)
        byteCount = 2;
    else if (additional == 26U)
        byteCount = 4;
    else if (additional == 27U)
        byteCount = 8;
    else
        return std::nullopt;

    if (m_input.size() - m_offset < byteCount)
        return std::nullopt;
    quint64 value = 0;
    for (int index = 0; index < byteCount; ++index)
        value = (value << 8U) | static_cast<quint8>(m_input.at(m_offset++));

    if ((byteCount == 1 && value < 24U) ||
        (byteCount == 2 && value <= std::numeric_limits<quint8>::max()) ||
        (byteCount == 4 && value <= std::numeric_limits<quint16>::max()) ||
        (byteCount == 8 && value <= std::numeric_limits<quint32>::max())) {
        return std::nullopt;
    }
    return value;
}

std::optional<QByteArray> DeterministicCborReader::readValue(quint8 expectedMajorType,
                                                             int maximumBytes) {
    const auto size = readHeader(expectedMajorType);
    if (!size || maximumBytes < 0 ||
        *size > static_cast<quint64>(maximumBytes) ||
        *size > static_cast<quint64>(m_input.size() - m_offset)) {
        return std::nullopt;
    }
    const QByteArray value = m_input.mid(m_offset, static_cast<int>(*size));
    m_offset += static_cast<int>(*size);
    return value;
}

}  // namespace easydesk::client::signaling::cbor
