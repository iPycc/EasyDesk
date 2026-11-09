#include "network/input/protocol/input_codec.h"

#include "network/input/protocol/wire/protobuf_wire.h"

#include <array>
#include <limits>
#include <type_traits>

namespace easydesk::client::input {
namespace {

constexpr int kTokenBytes = 16;
constexpr int kMaximumReliableBytes = 16384;
constexpr int kMaximumPointerBytes = 256;
constexpr int kMaximumTextCodeUnits = 256;
constexpr quint64 kMaximumCoordinate = 65535;

bool contextValid(const InputCommandContext &context) {
    return context.sessionId.size() == kTokenBytes && context.leaseId.size() == kTokenBytes &&
           context.sessionEpoch != 0 && context.permissionEpoch != 0 &&
           context.sequence != 0 && context.displayGeneration != 0 &&
           (context.stream == InputStream::Reliable || context.stream == InputStream::Pointer);
}

bool validText(const QString &text) {
    if (text.isEmpty() || text.size() > kMaximumTextCodeUnits || text.contains(QChar::Null))
        return false;
    const QByteArray utf8 = text.toUtf8();
    return QString::fromUtf8(utf8) == text;
}

bool coordinatesValid(quint64 x, quint64 y) {
    return x <= kMaximumCoordinate && y <= kMaximumCoordinate;
}

bool scalarFields(const QByteArray &bytes, int maxField, std::array<quint64, 4> &values) {
    wire::Reader reader(bytes);
    wire::Field field;
    quint32 seen = 0;
    while (!reader.atEnd()) {
        if (!reader.next(field) || field.type != 0 || field.number > static_cast<quint32>(maxField))
            return false;
        const quint32 bit = 1U << (field.number - 1);
        if (seen & bit)
            return false;
        seen |= bit;
        values[field.number - 1] = field.value;
    }
    return true;
}

quint32 zigzag(qint32 value) {
    return (static_cast<quint32>(value) << 1U) ^ (value < 0 ? 0xffffffffU : 0U);
}

qint32 unzigzag(quint32 value) {
    const qint64 result = (value & 1U) ? -static_cast<qint64>((value >> 1U) + 1U)
                                       : static_cast<qint64>(value >> 1U);
    return static_cast<qint32>(result);
}

bool encodeEvent(const InputCommand &command, quint32 &tag, QByteArray &body) {
    wire::Writer writer;
    const bool success = std::visit([&](const auto &event) -> bool {
        using Event = std::decay_t<decltype(event)>;
        if constexpr (std::is_same_v<Event, PointerMoveInput>) {
            if (command.context.stream != InputStream::Pointer ||
                !coordinatesValid(event.normalizedX, event.normalizedY))
                return false;
            tag = 10;
            writer.writeVarint(1, event.normalizedX);
            writer.writeVarint(2, event.normalizedY);
        } else if constexpr (std::is_same_v<Event, KeyInput>) {
            if (command.context.stream != InputStream::Reliable || event.scanCode == 0 ||
                (event.repeat && !event.pressed))
                return false;
            tag = 11;
            writer.writeVarint(1, event.scanCode);
            writer.writeVarint(2, event.extended);
            writer.writeVarint(3, event.pressed);
            writer.writeVarint(4, event.repeat);
        } else if constexpr (std::is_same_v<Event, PointerButtonInput>) {
            const auto button = static_cast<unsigned>(event.button);
            if (command.context.stream != InputStream::Reliable || button > 4 ||
                !coordinatesValid(event.normalizedX, event.normalizedY))
                return false;
            tag = 12;
            writer.writeVarint(1, button + 1);
            writer.writeVarint(2, event.pressed);
            writer.writeVarint(3, event.normalizedX);
            writer.writeVarint(4, event.normalizedY);
        } else if constexpr (std::is_same_v<Event, TextCommitInput>) {
            if (command.context.stream != InputStream::Reliable || !validText(event.text))
                return false;
            tag = 13;
            writer.writeBytes(1, event.text.toUtf8());
        } else if constexpr (std::is_same_v<Event, ReleaseAllInput>) {
            if (command.context.stream != InputStream::Reliable)
                return false;
            tag = 15;
        } else if constexpr (std::is_same_v<Event, WheelInput>) {
            if (command.context.stream != InputStream::Reliable ||
                (event.horizontalDelta == 0 && event.verticalDelta == 0) ||
                !coordinatesValid(event.normalizedX, event.normalizedY))
                return false;
            tag = 16;
            writer.writeVarint(1, zigzag(event.horizontalDelta));
            writer.writeVarint(2, zigzag(event.verticalDelta));
            writer.writeVarint(3, event.normalizedX);
            writer.writeVarint(4, event.normalizedY);
        }
        return true;
    }, command.event);
    if (success)
        body = writer.take();
    return success;
}

bool decodeEvent(quint32 tag, const QByteArray &body, InputStream stream, InputEvent &event) {
    if ((tag == 10) != (stream == InputStream::Pointer))
        return false;
    if (tag == 15) {
        if (!body.isEmpty())
            return false;
        event = ReleaseAllInput{};
        return true;
    }
    if (tag == 13) {
        wire::Reader reader(body);
        wire::Field field;
        if (reader.atEnd() || !reader.next(field) || field.number != 1 || field.type != 2 ||
            !reader.atEnd())
            return false;
        const QString text = QString::fromUtf8(field.bytes);
        if (!validText(text) || text.toUtf8() != field.bytes)
            return false;
        event = TextCommitInput{text};
        return true;
    }
    if (tag != 10 && tag != 11 && tag != 12 && tag != 16)
        return false; // InputState is reserved until reconciliation is implemented.
    std::array<quint64, 4> values{};
    if (!scalarFields(body, tag == 10 ? 2 : 4, values))
        return false;
    if (tag == 10) {
        if (!coordinatesValid(values[0], values[1]))
            return false;
        event = PointerMoveInput{static_cast<quint32>(values[0]), static_cast<quint32>(values[1])};
    } else if (tag == 11) {
        if (values[0] == 0 || values[0] > 65535 || values[1] > 1 || values[2] > 1 ||
            values[3] > 1 || (values[3] && !values[2]))
            return false;
        event = KeyInput{static_cast<quint16>(values[0]), values[1] != 0,
                         values[2] != 0, values[3] != 0};
    } else if (tag == 12) {
        if (values[0] < 1 || values[0] > 5 || values[1] > 1 ||
            !coordinatesValid(values[2], values[3]))
            return false;
        event = PointerButtonInput{static_cast<RemoteMouseButton>(values[0] - 1), values[1] != 0,
                                   static_cast<quint32>(values[2]), static_cast<quint32>(values[3])};
    } else {
        if (values[0] > std::numeric_limits<quint32>::max() ||
            values[1] > std::numeric_limits<quint32>::max() ||
            (values[0] == 0 && values[1] == 0) || !coordinatesValid(values[2], values[3]))
            return false;
        event = WheelInput{unzigzag(static_cast<quint32>(values[0])),
                           unzigzag(static_cast<quint32>(values[1])),
                           static_cast<quint32>(values[2]), static_cast<quint32>(values[3])};
    }
    return true;
}

}  // namespace

bool encodeInputCommand(const InputCommand &command, QByteArray &output, QString &error) {
    output.clear();
    quint32 tag = 0;
    QByteArray body;
    if (!contextValid(command.context) || !encodeEvent(command, tag, body)) {
        error = QStringLiteral("Invalid input command or context.");
        return false;
    }
    wire::Writer writer;
    writer.writeVarint(1, 1);
    writer.writeBytes(2, command.context.sessionId);
    writer.writeVarint(3, command.context.sessionEpoch);
    writer.writeVarint(4, command.context.permissionEpoch);
    writer.writeVarint(5, command.context.sequence);
    writer.writeVarint(6, command.context.displayGeneration);
    writer.writeBytes(7, command.context.leaseId);
    writer.writeBytes(tag, body);
    output = writer.take();
    const int maximum = command.context.stream == InputStream::Pointer
                            ? kMaximumPointerBytes : kMaximumReliableBytes;
    if (output.size() > maximum) {
        output.clear();
        error = QStringLiteral("Input message exceeds the channel limit.");
        return false;
    }
    error.clear();
    return true;
}

bool decodeInputCommand(const QByteArray &data, InputStream stream,
                        InputCommand &command, QString &error) {
    const int maximum = stream == InputStream::Pointer ? kMaximumPointerBytes
                                                         : kMaximumReliableBytes;
    if ((stream != InputStream::Pointer && stream != InputStream::Reliable) ||
        data.isEmpty() || data.size() > maximum) {
        error = QStringLiteral("Invalid input message size or stream.");
        return false;
    }
    InputCommand decoded;
    decoded.context.stream = stream;
    wire::Reader reader(data);
    wire::Field field;
    quint32 seen = 0;
    quint32 eventTag = 0;
    QByteArray eventBody;
    while (!reader.atEnd()) {
        if (!reader.next(field) || field.number > 16 || field.number == 8 || field.number == 9) {
            error = QStringLiteral("Malformed or unknown input field.");
            return false;
        }
        const quint32 bit = 1U << (field.number - 1);
        if (seen & bit) {
            error = QStringLiteral("Duplicate input field.");
            return false;
        }
        seen |= bit;
        if (field.number >= 10) {
            if (field.type != 2 || eventTag != 0) {
                error = QStringLiteral("Invalid input event.");
                return false;
            }
            eventTag = field.number;
            eventBody = field.bytes;
        } else if (field.number == 2 || field.number == 7) {
            if (field.type != 2) {
                error = QStringLiteral("Invalid input token.");
                return false;
            }
            (field.number == 2 ? decoded.context.sessionId : decoded.context.leaseId) = field.bytes;
        } else {
            if (field.type != 0 || (field.number == 1 && field.value != 1)) {
                error = QStringLiteral("Invalid input context field.");
                return false;
            }
            switch (field.number) {
            case 1: break;
            case 3: decoded.context.sessionEpoch = field.value; break;
            case 4: decoded.context.permissionEpoch = field.value; break;
            case 5: decoded.context.sequence = field.value; break;
            case 6: decoded.context.displayGeneration = field.value; break;
            default: return false;
            }
        }
    }
    if ((seen & 0x7fU) != 0x7fU || !contextValid(decoded.context) || eventTag == 0 ||
        !decodeEvent(eventTag, eventBody, stream, decoded.event)) {
        error = QStringLiteral("Invalid input event or context.");
        return false;
    }
    command = std::move(decoded);
    error.clear();
    return true;
}

}  // namespace easydesk::client::input
