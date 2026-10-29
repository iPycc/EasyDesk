#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace easydesk::protocol {

struct SignedEnvelope {
    std::string type;
    std::string targetId;
    std::string sessionId;
    std::uint32_t sessionEpoch;
    std::uint64_t sequence;
    std::int64_t expiresAt;
    std::vector<std::uint8_t> payload;
    std::vector<std::uint8_t> signature;
};

std::vector<std::uint8_t> envelopeSigningBytes(const SignedEnvelope& envelope,
                                               const std::string& sourceDeviceId);
bool verifySignedEnvelope(const SignedEnvelope& envelope,
                          const std::string& sourceDeviceId,
                          const std::vector<std::uint8_t>& publicKey);

}  // namespace easydesk::protocol
