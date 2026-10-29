#include "protocol/signaling/signed_envelope.hpp"

#include "security/crypto/ed25519.hpp"

#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace easydesk::protocol {
namespace {

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void appendU64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>((value >> static_cast<unsigned int>(shift)) &
                                                   0xffU));
    }
}

void appendBytes(std::vector<std::uint8_t>& output, std::span<const std::uint8_t> value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("signed envelope field is too large");
    }
    appendU32(output, static_cast<std::uint32_t>(value.size()));
    output.insert(output.end(), value.begin(), value.end());
}

void appendString(std::vector<std::uint8_t>& output, std::string_view value) {
    appendBytes(output, {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
}

}  // namespace

std::vector<std::uint8_t> envelopeSigningBytes(const SignedEnvelope& envelope,
                                               const std::string& sourceDeviceId) {
    constexpr std::string_view domain = "EasyDesk-Signal-v1";
    const auto payloadHash = security::sha256Digest(envelope.payload);

    std::vector<std::uint8_t> output;
    output.reserve(160 + envelope.type.size() + envelope.sessionId.size());
    output.insert(output.end(), domain.begin(), domain.end());
    output.push_back(0);
    appendString(output, sourceDeviceId);
    appendString(output, envelope.targetId);
    appendString(output, envelope.type);
    appendString(output, envelope.sessionId);
    appendU32(output, envelope.sessionEpoch);
    appendU64(output, envelope.sequence);
    appendU64(output, static_cast<std::uint64_t>(envelope.expiresAt));
    appendBytes(output, payloadHash);
    return output;
}

bool verifySignedEnvelope(const SignedEnvelope& envelope,
                          const std::string& sourceDeviceId,
                          const std::vector<std::uint8_t>& publicKey) {
    if (publicKey.size() != 32 || envelope.signature.size() != 64 || envelope.expiresAt <= 0) {
        return false;
    }
    const auto message = envelopeSigningBytes(envelope, sourceDeviceId);
    return security::verifyEd25519(publicKey, message, envelope.signature);
}

}  // namespace easydesk::protocol
