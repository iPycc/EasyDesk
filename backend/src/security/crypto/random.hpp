#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace easydesk::security {

std::vector<std::uint8_t> randomBytes(std::size_t size);

}  // namespace easydesk::security
