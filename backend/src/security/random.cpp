#include "security/random.hpp"

#include <openssl/rand.h>

#include <limits>
#include <stdexcept>

namespace easydesk::security {

std::vector<std::uint8_t> randomBytes(std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("random byte request is too large");
    }

    std::vector<std::uint8_t> bytes(size);
    if (size != 0 && RAND_bytes(bytes.data(), static_cast<int>(size)) != 1) {
        throw std::runtime_error("secure random generation failed");
    }
    return bytes;
}

}  // namespace easydesk::security
