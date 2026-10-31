#include "network/signaling/protocol/encoding/base64url.h"

namespace easydesk::client::signaling {

QByteArray encodeBase64Url(const QByteArray &value) {
    return value.toBase64(QByteArray::Base64UrlEncoding |
                          QByteArray::OmitTrailingEquals);
}

std::optional<QByteArray> decodeBase64Url(const QByteArray &value) {
    if (value.contains('=') || value.size() % 4 == 1)
        return std::nullopt;
    for (const char character : value) {
        const bool alphaNumeric =
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9');
        if (!alphaNumeric && character != '-' && character != '_')
            return std::nullopt;
    }

    const auto decoded = QByteArray::fromBase64Encoding(
        value, QByteArray::Base64UrlEncoding |
                   QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || encodeBase64Url(decoded.decoded) != value)
        return std::nullopt;
    return decoded.decoded;
}

}  // namespace easydesk::client::signaling
