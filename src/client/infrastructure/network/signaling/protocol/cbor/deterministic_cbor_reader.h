#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace easydesk::client::signaling::cbor {

class DeterministicCborReader final {
public:
    explicit DeterministicCborReader(const QByteArray &input);

    std::optional<quint64> readArraySize();
    std::optional<quint64> readUnsigned();
    std::optional<QByteArray> readBytes(int maximumBytes);
    std::optional<QString> readText(int maximumBytes);
    bool atEnd() const;

private:
    std::optional<quint64> readHeader(quint8 expectedMajorType);
    std::optional<QByteArray> readValue(quint8 expectedMajorType,
                                        int maximumBytes);

    QByteArray m_input;
    int m_offset = 0;
};

}  // namespace easydesk::client::signaling::cbor
