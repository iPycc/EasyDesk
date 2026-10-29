#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace easydesk::protocol {

struct RegistrationEnvelope {
    std::string deviceId;
    std::vector<std::uint8_t> publicKey;
    std::string displayName;
    std::string platform;
    std::uint32_t schemaVersion;
    std::int64_t timestamp;
    std::string nonce;
    std::vector<std::uint8_t> signature;
};

std::vector<std::uint8_t> registrationSigningBytes(const RegistrationEnvelope& envelope);
bool verifyRegistrationEnvelope(const RegistrationEnvelope& envelope);

}  // namespace easydesk::protocol
