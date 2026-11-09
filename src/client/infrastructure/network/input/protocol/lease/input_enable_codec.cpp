#include "network/input/protocol/lease/input_enable_codec.h"

#include "network/input/protocol/wire/protobuf_wire.h"

namespace easydesk::client::input {
namespace {
constexpr auto kPrefix = "EDE1";
constexpr int kMaximumBytes = 128;
constexpr quint32 kRequiredFields = (1U << 6U) - 1U;

bool valid(const InputEnableRequest &request) {
    return request.sessionId.size() == 16 && request.sessionEpoch != 0 &&
           request.permissionEpoch != 0 && request.displayGeneration != 0 && request.sequence != 0;
}
}  // namespace

bool isInputEnableRequest(const QByteArray &data) {
    return data.startsWith(kPrefix);
}

bool encodeInputEnableRequest(const InputEnableRequest &request, QByteArray &output, QString &error) {
    output.clear();
    if (!valid(request)) {
        error = QStringLiteral("Input enable request is invalid.");
        return false;
    }
    wire::Writer writer;
    writer.writeVarint(1, 1);
    writer.writeBytes(2, request.sessionId);
    writer.writeVarint(3, request.sessionEpoch);
    writer.writeVarint(4, request.permissionEpoch);
    writer.writeVarint(5, request.displayGeneration);
    writer.writeVarint(6, request.sequence);
    output = QByteArray(kPrefix) + writer.take();
    error.clear();
    return true;
}

bool decodeInputEnableRequest(const QByteArray &data, InputEnableRequest &request, QString &error) {
    request = {};
    if (!isInputEnableRequest(data) || data.size() > kMaximumBytes) {
        error = QStringLiteral("Input enable request framing is invalid.");
        return false;
    }
    wire::Reader reader(data.mid(4));
    wire::Field field;
    InputEnableRequest decoded;
    quint32 seen = 0;
    quint64 version = 0;
    while (!reader.atEnd()) {
        if (!reader.next(field) || field.number < 1 || field.number > 6) {
            error = QStringLiteral("Input enable request field is invalid.");
            return false;
        }
        const quint32 bit = 1U << (field.number - 1);
        if ((seen & bit) != 0 || field.type != (field.number == 2 ? 2 : 0)) {
            error = QStringLiteral("Input enable request fields are malformed.");
            return false;
        }
        seen |= bit;
        switch (field.number) {
        case 1: version = field.value; break;
        case 2: decoded.sessionId = field.bytes; break;
        case 3: decoded.sessionEpoch = field.value; break;
        case 4: decoded.permissionEpoch = field.value; break;
        case 5: decoded.displayGeneration = field.value; break;
        case 6: decoded.sequence = field.value; break;
        }
    }
    if (seen != kRequiredFields || version != 1 || !valid(decoded)) {
        error = QStringLiteral("Input enable request fields are invalid.");
        return false;
    }
    request = decoded;
    error.clear();
    return true;
}

}  // namespace easydesk::client::input
