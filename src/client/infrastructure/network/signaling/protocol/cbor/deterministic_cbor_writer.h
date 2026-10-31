#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::signaling::cbor {

class DeterministicCborWriter final {
public:
    void beginArray(quint64 size);
    void writeUnsigned(quint64 value);
    void writeBytes(const QByteArray &value);
    void writeText(const QString &value);

    const QByteArray &bytes() const;
    QByteArray takeBytes();

private:
    void writeHeader(quint8 majorType, quint64 value);

    QByteArray output_;
};

}  // namespace easydesk::client::signaling::cbor
