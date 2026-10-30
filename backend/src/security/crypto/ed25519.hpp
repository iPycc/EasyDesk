#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace easydesk::security {

std::array<std::uint8_t, 32> sha256Digest(std::span<const std::uint8_t> bytes);
std::string sha256Fingerprint(std::span<const std::uint8_t> publicKey);

bool verifyEd25519(std::span<const std::uint8_t> publicKey,
                   std::span<const std::uint8_t> message,
                   std::span<const std::uint8_t> signature);

}  // namespace easydesk::security
#include <array>
