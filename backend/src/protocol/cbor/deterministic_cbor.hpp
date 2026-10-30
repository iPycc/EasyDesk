#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace easydesk::protocol::cbor {

class Writer final {
public:
    void beginArray(std::size_t size);
    void writeUnsigned(std::uint64_t value);
    void writeBytes(std::span<const std::uint8_t> value);
    void writeText(std::string_view value);

    const std::vector<std::uint8_t>& bytes() const;
    std::vector<std::uint8_t> takeBytes();

private:
    void writeHeader(std::uint8_t majorType, std::uint64_t value);

    std::vector<std::uint8_t> output_;
};

class Reader final {
public:
    explicit Reader(std::span<const std::uint8_t> input);

    std::optional<std::size_t> readArraySize();
    std::optional<std::uint64_t> readUnsigned();
    std::optional<std::vector<std::uint8_t>> readBytes(std::size_t maximumBytes);
    std::optional<std::string> readText(std::size_t maximumBytes);
    bool finished() const;

private:
    std::optional<std::uint64_t> readHeader(std::uint8_t expectedMajorType);
    std::optional<std::span<const std::uint8_t>> readValue(std::uint8_t expectedMajorType,
                                                           std::size_t maximumBytes);

    std::span<const std::uint8_t> input_;
    std::size_t offset_ = 0;
};

bool isValidUtf8(std::string_view value);

}  // namespace easydesk::protocol::cbor
