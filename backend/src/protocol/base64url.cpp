#include "protocol/base64url.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <limits>

namespace easydesk::protocol {
namespace {

bool isBase64UrlCharacter(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '-' || value == '_';
}

}  // namespace

std::string encodeBase64Url(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) {
        return {};
    }
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    std::string encoded(4 * ((bytes.size() + 2) / 3), '\0');
    const int length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), bytes.data(),
                                       static_cast<int>(bytes.size()));
    if (length < 0) {
        return {};
    }
    encoded.resize(static_cast<std::size_t>(length));
    std::replace(encoded.begin(), encoded.end(), '+', '-');
    std::replace(encoded.begin(), encoded.end(), '/', '_');
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

std::optional<std::vector<std::uint8_t>> decodeBase64Url(std::string_view text) {
    if (text.empty()) {
        return std::vector<std::uint8_t>();
    }
    if (text.size() % 4 == 1 ||
        !std::all_of(text.begin(), text.end(), [](char value) { return isBase64UrlCharacter(value); })) {
        return std::nullopt;
    }

    std::string padded(text);
    std::replace(padded.begin(), padded.end(), '-', '+');
    std::replace(padded.begin(), padded.end(), '_', '/');
    while (padded.size() % 4 != 0) {
        padded.push_back('=');
    }
    if (padded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> decoded(3 * (padded.size() / 4));
    const int length = EVP_DecodeBlock(decoded.data(),
                                       reinterpret_cast<const unsigned char*>(padded.data()),
                                       static_cast<int>(padded.size()));
    if (length < 0) {
        return std::nullopt;
    }

    std::size_t padding = 0;
    if (!padded.empty() && padded.back() == '=') {
        ++padding;
    }
    if (padded.size() > 1 && padded[padded.size() - 2] == '=') {
        ++padding;
    }
    if (static_cast<std::size_t>(length) < padding) {
        return std::nullopt;
    }
    decoded.resize(static_cast<std::size_t>(length) - padding);
    return decoded;
}

}  // namespace easydesk::protocol
