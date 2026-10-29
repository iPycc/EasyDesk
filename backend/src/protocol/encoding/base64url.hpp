#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace easydesk::protocol {

std::string encodeBase64Url(std::span<const std::uint8_t> bytes);
std::optional<std::vector<std::uint8_t>> decodeBase64Url(std::string_view text);

}  // namespace easydesk::protocol
