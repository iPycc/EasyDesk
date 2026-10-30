#include "protocol/session/session_payload.hpp"

#include "security/crypto/ed25519.hpp"

namespace easydesk::protocol {

SessionDigest sessionPayloadDigest(std::span<const std::uint8_t> signedPayload) {
    return security::sha256Digest(signedPayload);
}

}  // namespace easydesk::protocol
