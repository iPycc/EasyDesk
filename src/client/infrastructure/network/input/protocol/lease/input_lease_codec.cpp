#include "network/input/protocol/lease/input_lease_codec.h"

#include "network/input/protocol/wire/protobuf_wire.h"

namespace easydesk::client::input {
namespace {
constexpr int kMaximumMessageBytes = 256;
constexpr quint32 kRequiredFields = (1U << 9U) - 1U;
constexpr auto kPrefix = "EDL1";

bool validMessage(const InputLeaseMessage &message) {
    const auto &context = message.context;
    if (context.stream != InputStream::Reliable || context.sessionId.size() != 16 ||
        context.leaseId.size() != 16 || context.sessionEpoch == 0 ||
        context.permissionEpoch == 0 || context.displayGeneration == 0 ||
        message.leaseGeneration == 0 || message.challenge.size() != 16)
        return false;
    if (message.kind == InputLeaseMessageKind::Challenge)
        return message.timeoutMilliseconds >= 1 && message.timeoutMilliseconds <= 1500;
    if (message.kind == InputLeaseMessageKind::Renewal)
        return context.sequence != 0 && message.timeoutMilliseconds == 0;
    return false;
}
}  // namespace

bool isInputLeaseMessage(const QByteArray &data) {
    return data.startsWith(kPrefix);
}

bool encodeInputLeaseMessage(const InputLeaseMessage &message, QByteArray &output, QString &error) {
    output.clear();
    if (!validMessage(message)) {
        error = QStringLiteral("Input lease message fields are invalid.");
        return false;
    }
    wire::Writer writer;
    writer.writeVarint(1, 1);
    writer.writeVarint(2, static_cast<quint8>(message.kind));
    writer.writeBytes(3, message.context.sessionId);
    writer.writeVarint(4, message.context.sessionEpoch);
    writer.writeVarint(5, message.context.permissionEpoch);
    writer.writeVarint(6, message.context.displayGeneration);
    writer.writeBytes(7, message.context.leaseId);
    writer.writeVarint(8, message.leaseGeneration);
    writer.writeBytes(9, message.challenge);
    writer.writeVarint(10, message.context.sequence);
    writer.writeVarint(11, message.timeoutMilliseconds);
    output = QByteArray(kPrefix) + writer.take();
    error.clear();
    return true;
}

bool decodeInputLeaseMessage(const QByteArray &data, InputLeaseMessage &message, QString &error) {
    message = {};
    InputLeaseMessage decoded;
    if (!isInputLeaseMessage(data) || data.size() > kMaximumMessageBytes) {
        error = QStringLiteral("Input lease message framing is invalid.");
        return false;
    }
    wire::Reader reader(data.mid(4));
    wire::Field field;
    quint32 seen = 0;
    quint64 version = 0, kind = 0, timeout = 0;
    while (!reader.atEnd()) {
        if (!reader.next(field) || field.number < 1 || field.number > 11) {
            error = QStringLiteral("Input lease message contains an invalid field.");
            return false;
        }
        const quint32 bit = 1U << (field.number - 1);
        const quint8 expectedType = (field.number == 3 || field.number == 7 || field.number == 9) ? 2 : 0;
        if ((seen & bit) != 0 || field.type != expectedType) {
            error = QStringLiteral("Input lease message fields are duplicated or malformed.");
            return false;
        }
        seen |= bit;
        switch (field.number) {
        case 1: version = field.value; break;
        case 2: kind = field.value; break;
        case 3: decoded.context.sessionId = field.bytes; break;
        case 4: decoded.context.sessionEpoch = field.value; break;
        case 5: decoded.context.permissionEpoch = field.value; break;
        case 6: decoded.context.displayGeneration = field.value; break;
        case 7: decoded.context.leaseId = field.bytes; break;
        case 8: decoded.leaseGeneration = field.value; break;
        case 9: decoded.challenge = field.bytes; break;
        case 10: decoded.context.sequence = field.value; break;
        case 11: timeout = field.value; break;
        }
    }
    if ((seen & kRequiredFields) != kRequiredFields || version != 1 ||
        kind < 1 || kind > 2 || timeout > 1500) {
        error = QStringLiteral("Input lease message is incomplete or unsupported.");
        return false;
    }
    decoded.kind = static_cast<InputLeaseMessageKind>(kind);
    decoded.timeoutMilliseconds = static_cast<quint32>(timeout);
    decoded.context.stream = InputStream::Reliable;
    if (!validMessage(decoded)) {
        error = QStringLiteral("Input lease message fields are invalid.");
        return false;
    }
    message = decoded;
    error.clear();
    return true;
}

}  // namespace easydesk::client::input
