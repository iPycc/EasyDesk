#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace easydesk::client::input::wire {

struct Field {
    quint32 number = 0;
    quint8 type = 0;
    quint64 value = 0;
    QByteArray bytes;
};

// The input protocol uses only unsigned varints and length-delimited fields.
class Reader final {
  public:
    explicit Reader(const QByteArray &input);
    bool next(Field &field);
    bool atEnd() const;

  private:
    bool readVarint(quint64 &value);
    QByteArray m_input;
    int m_offset = 0;
};

class Writer final {
  public:
    void writeVarint(quint32 fieldNumber, quint64 value);
    void writeBytes(quint32 fieldNumber, const QByteArray &value);
    QByteArray take();

  private:
    void appendVarint(quint64 value);
    QByteArray m_output;
};

}  // namespace easydesk::client::input::wire
