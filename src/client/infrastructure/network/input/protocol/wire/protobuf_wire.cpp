#include "network/input/protocol/wire/protobuf_wire.h"

#include <utility>

namespace easydesk::client::input::wire {

Reader::Reader(const QByteArray &input) : m_input(input) {}

bool Reader::readVarint(quint64 &value) {
    value = 0;
    for (int index = 0; index < 10; ++index) {
        if (m_offset >= m_input.size())
            return false;
        const quint8 byte = static_cast<quint8>(m_input.at(m_offset++));
        if (index == 9 && byte > 1)
            return false;
        value |= static_cast<quint64>(byte & 0x7fU) << (index * 7);
        if ((byte & 0x80U) == 0)
            return index == 0 || byte != 0;
    }
    return false;
}

bool Reader::next(Field &field) {
    field = {};
    quint64 key = 0;
    if (!readVarint(key) || (key >> 3) == 0 || (key >> 3) > 0x1fffffffU)
        return false;
    field.number = static_cast<quint32>(key >> 3);
    field.type = static_cast<quint8>(key & 7U);
    if (field.type == 0)
        return readVarint(field.value);
    if (field.type != 2)
        return false;
    quint64 length = 0;
    if (!readVarint(length) || length > static_cast<quint64>(m_input.size() - m_offset))
        return false;
    field.bytes = m_input.mid(m_offset, static_cast<int>(length));
    m_offset += static_cast<int>(length);
    return true;
}

bool Reader::atEnd() const {
    return m_offset == m_input.size();
}

void Writer::appendVarint(quint64 value) {
    while (value >= 0x80U) {
        m_output.append(static_cast<char>((value & 0x7fU) | 0x80U));
        value >>= 7;
    }
    m_output.append(static_cast<char>(value));
}

void Writer::writeVarint(quint32 fieldNumber, quint64 value) {
    appendVarint(static_cast<quint64>(fieldNumber) << 3);
    appendVarint(value);
}

void Writer::writeBytes(quint32 fieldNumber, const QByteArray &value) {
    appendVarint((static_cast<quint64>(fieldNumber) << 3) | 2U);
    appendVarint(static_cast<quint64>(value.size()));
    m_output.append(value);
}

QByteArray Writer::take() {
    return std::move(m_output);
}

}  // namespace easydesk::client::input::wire
