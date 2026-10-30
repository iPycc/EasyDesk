#include "protocol/cbor/deterministic_cbor.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace easydesk::protocol::cbor {
namespace {

constexpr std::uint8_t kUnsignedMajorType = 0;
constexpr std::uint8_t kBytesMajorType = 2;
constexpr std::uint8_t kTextMajorType = 3;
constexpr std::uint8_t kArrayMajorType = 4;

bool isContinuation(std::uint8_t value) {
    return value >= 0x80 && value <= 0xbf;
}

}  // namespace

void Writer::beginArray(std::size_t size) {
    writeHeader(kArrayMajorType, static_cast<std::uint64_t>(size));
}

void Writer::writeUnsigned(std::uint64_t value) {
    writeHeader(kUnsignedMajorType, value);
}

void Writer::writeBytes(std::span<const std::uint8_t> value) {
    writeHeader(kBytesMajorType, static_cast<std::uint64_t>(value.size()));
    output_.insert(output_.end(), value.begin(), value.end());
}

void Writer::writeText(std::string_view value) {
    if (!isValidUtf8(value))
        throw std::invalid_argument("CBOR text must contain valid UTF-8");
    writeHeader(kTextMajorType, static_cast<std::uint64_t>(value.size()));
    output_.insert(output_.end(), value.begin(), value.end());
}

const std::vector<std::uint8_t>& Writer::bytes() const {
    return output_;
}

std::vector<std::uint8_t> Writer::takeBytes() {
    return std::move(output_);
}

void Writer::writeHeader(std::uint8_t majorType, std::uint64_t value) {
    const std::uint8_t prefix = static_cast<std::uint8_t>(majorType << 5U);
    if (value < 24) {
        output_.push_back(static_cast<std::uint8_t>(prefix | value));
        return;
    }
    if (value <= std::numeric_limits<std::uint8_t>::max()) {
        output_.push_back(static_cast<std::uint8_t>(prefix | 24U));
        output_.push_back(static_cast<std::uint8_t>(value));
        return;
    }
    if (value <= std::numeric_limits<std::uint16_t>::max()) {
        output_.push_back(static_cast<std::uint8_t>(prefix | 25U));
        output_.push_back(static_cast<std::uint8_t>(value >> 8U));
        output_.push_back(static_cast<std::uint8_t>(value));
        return;
    }
    if (value <= std::numeric_limits<std::uint32_t>::max()) {
        output_.push_back(static_cast<std::uint8_t>(prefix | 26U));
        for (int shift = 24; shift >= 0; shift -= 8)
            output_.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
        return;
    }
    output_.push_back(static_cast<std::uint8_t>(prefix | 27U));
    for (int shift = 56; shift >= 0; shift -= 8)
        output_.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
}

Reader::Reader(std::span<const std::uint8_t> input) : input_(input) {}

std::optional<std::size_t> Reader::readArraySize() {
    const auto size = readHeader(kArrayMajorType);
    if (!size || *size > std::numeric_limits<std::size_t>::max())
        return std::nullopt;
    return static_cast<std::size_t>(*size);
}

std::optional<std::uint64_t> Reader::readUnsigned() {
    return readHeader(kUnsignedMajorType);
}

std::optional<std::vector<std::uint8_t>> Reader::readBytes(std::size_t maximumBytes) {
    const auto value = readValue(kBytesMajorType, maximumBytes);
    if (!value)
        return std::nullopt;
    return std::vector<std::uint8_t>(value->begin(), value->end());
}

std::optional<std::string> Reader::readText(std::size_t maximumBytes) {
    const auto value = readValue(kTextMajorType, maximumBytes);
    if (!value)
        return std::nullopt;
    std::string text(reinterpret_cast<const char*>(value->data()), value->size());
    return isValidUtf8(text) ? std::optional<std::string>(std::move(text)) : std::nullopt;
}

bool Reader::finished() const {
    return offset_ == input_.size();
}

std::optional<std::uint64_t> Reader::readHeader(std::uint8_t expectedMajorType) {
    if (offset_ >= input_.size())
        return std::nullopt;
    const std::uint8_t initial = input_[offset_++];
    if ((initial >> 5U) != expectedMajorType)
        return std::nullopt;

    const std::uint8_t additional = initial & 0x1fU;
    if (additional < 24)
        return additional;
    std::size_t byteCount = 0;
    if (additional == 24)
        byteCount = 1;
    else if (additional == 25)
        byteCount = 2;
    else if (additional == 26)
        byteCount = 4;
    else if (additional == 27)
        byteCount = 8;
    else
        return std::nullopt;

    if (input_.size() - offset_ < byteCount)
        return std::nullopt;
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
        value = (value << 8U) | input_[offset_++];

    if ((byteCount == 1 && value < 24) ||
        (byteCount == 2 && value <= std::numeric_limits<std::uint8_t>::max()) ||
        (byteCount == 4 && value <= std::numeric_limits<std::uint16_t>::max()) ||
        (byteCount == 8 && value <= std::numeric_limits<std::uint32_t>::max())) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::span<const std::uint8_t>> Reader::readValue(
    std::uint8_t expectedMajorType,
    std::size_t maximumBytes) {
    const auto size = readHeader(expectedMajorType);
    if (!size || *size > maximumBytes || *size > input_.size() - offset_)
        return std::nullopt;
    const auto value = input_.subspan(offset_, static_cast<std::size_t>(*size));
    offset_ += static_cast<std::size_t>(*size);
    return value;
}

bool isValidUtf8(std::string_view value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(value.data());
    std::size_t index = 0;
    while (index < value.size()) {
        const std::uint8_t first = bytes[index++];
        if (first <= 0x7f)
            continue;
        if (first >= 0xc2 && first <= 0xdf) {
            if (index >= value.size() || !isContinuation(bytes[index++]))
                return false;
            continue;
        }
        if (first >= 0xe0 && first <= 0xef) {
            if (value.size() - index < 2)
                return false;
            const std::uint8_t second = bytes[index++];
            const std::uint8_t third = bytes[index++];
            if (!isContinuation(third) ||
                (first == 0xe0 ? second < 0xa0 || second > 0xbf
                               : first == 0xed ? second < 0x80 || second > 0x9f
                                               : !isContinuation(second))) {
                return false;
            }
            continue;
        }
        if (first >= 0xf0 && first <= 0xf4) {
            if (value.size() - index < 3)
                return false;
            const std::uint8_t second = bytes[index++];
            const std::uint8_t third = bytes[index++];
            const std::uint8_t fourth = bytes[index++];
            if (!isContinuation(third) || !isContinuation(fourth) ||
                (first == 0xf0 ? second < 0x90 || second > 0xbf
                               : first == 0xf4 ? second < 0x80 || second > 0x8f
                                               : !isContinuation(second))) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace easydesk::protocol::cbor
