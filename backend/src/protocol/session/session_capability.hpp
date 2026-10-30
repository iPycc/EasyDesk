#pragma once

#include "protocol/cbor/deterministic_cbor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace easydesk::protocol {

inline constexpr std::size_t kSessionPublicKeySize = 32;
inline constexpr std::size_t kMaximumCapabilityGrants = 4;

using SessionPublicKey = std::array<std::uint8_t, kSessionPublicKeySize>;

enum class SessionCapability : std::uint8_t {
    viewScreen = 1,
    sendInput = 2,
    fileReceive = 3,
};

struct CapabilityGrant {
    SessionPublicKey subjectPublicKey;
    SessionCapability capability;

    bool operator==(const CapabilityGrant&) const = default;
};

bool capabilityGrantLess(const CapabilityGrant& left, const CapabilityGrant& right);
bool validateCapabilityGrants(const std::vector<CapabilityGrant>& grants,
                              const SessionPublicKey& controllerPublicKey,
                              const SessionPublicKey& controlledPublicKey);
bool capabilityGrantsAreSubset(const std::vector<CapabilityGrant>& granted,
                               const std::vector<CapabilityGrant>& requested);

void writeCapabilityGrants(cbor::Writer& writer,
                           const std::vector<CapabilityGrant>& grants);
std::optional<std::vector<CapabilityGrant>> readCapabilityGrants(
    cbor::Reader& reader,
    const SessionPublicKey& controllerPublicKey,
    const SessionPublicKey& controlledPublicKey);

}  // namespace easydesk::protocol
