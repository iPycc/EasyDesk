#include "protocol/session/session_capability.hpp"

#include <algorithm>
#include <utility>

namespace easydesk::protocol {
namespace {

bool isKnownCapability(SessionCapability capability) {
    return capability == SessionCapability::viewScreen ||
           capability == SessionCapability::sendInput ||
           capability == SessionCapability::fileReceive;
}

bool isParticipant(const SessionPublicKey& subject,
                   const SessionPublicKey& controller,
                   const SessionPublicKey& controlled) {
    return subject == controller || subject == controlled;
}

}  // namespace

bool capabilityGrantLess(const CapabilityGrant& left, const CapabilityGrant& right) {
    if (left.subjectPublicKey != right.subjectPublicKey)
        return left.subjectPublicKey < right.subjectPublicKey;
    return static_cast<std::uint8_t>(left.capability) <
           static_cast<std::uint8_t>(right.capability);
}

bool validateCapabilityGrants(const std::vector<CapabilityGrant>& grants,
                              const SessionPublicKey& controllerPublicKey,
                              const SessionPublicKey& controlledPublicKey) {
    if (controllerPublicKey == controlledPublicKey || grants.empty() ||
        grants.size() > kMaximumCapabilityGrants)
        return false;

    bool controllerMayView = false;
    bool controllerMaySendInput = false;
    for (std::size_t index = 0; index < grants.size(); ++index) {
        const CapabilityGrant& grant = grants[index];
        if (!isKnownCapability(grant.capability) ||
            !isParticipant(grant.subjectPublicKey, controllerPublicKey, controlledPublicKey) ||
            (index > 0 && !capabilityGrantLess(grants[index - 1], grant))) {
            return false;
        }
        if ((grant.capability == SessionCapability::viewScreen ||
             grant.capability == SessionCapability::sendInput) &&
            grant.subjectPublicKey != controllerPublicKey) {
            return false;
        }
        controllerMayView = controllerMayView ||
                            grant.capability == SessionCapability::viewScreen;
        controllerMaySendInput = controllerMaySendInput ||
                                 grant.capability == SessionCapability::sendInput;
    }
    return !controllerMaySendInput || controllerMayView;
}

bool capabilityGrantsAreSubset(const std::vector<CapabilityGrant>& granted,
                               const std::vector<CapabilityGrant>& requested) {
    return std::includes(requested.begin(), requested.end(), granted.begin(), granted.end(),
                         capabilityGrantLess);
}

void writeCapabilityGrants(cbor::Writer& writer,
                           const std::vector<CapabilityGrant>& grants) {
    writer.beginArray(grants.size());
    for (const CapabilityGrant& grant : grants) {
        writer.beginArray(2);
        writer.writeBytes(grant.subjectPublicKey);
        writer.writeUnsigned(static_cast<std::uint8_t>(grant.capability));
    }
}

std::optional<std::vector<CapabilityGrant>> readCapabilityGrants(
    cbor::Reader& reader,
    const SessionPublicKey& controllerPublicKey,
    const SessionPublicKey& controlledPublicKey) {
    const auto count = reader.readArraySize();
    if (!count || *count == 0 || *count > kMaximumCapabilityGrants)
        return std::nullopt;

    std::vector<CapabilityGrant> grants;
    grants.reserve(*count);
    for (std::size_t index = 0; index < *count; ++index) {
        const auto entrySize = reader.readArraySize();
        auto subject = reader.readBytes(kSessionPublicKeySize);
        const auto capabilityValue = reader.readUnsigned();
        if (!entrySize || *entrySize != 2 || !subject ||
            subject->size() != kSessionPublicKeySize || !capabilityValue ||
            *capabilityValue > static_cast<std::uint8_t>(SessionCapability::fileReceive)) {
            return std::nullopt;
        }
        SessionPublicKey subjectPublicKey{};
        std::copy(subject->begin(), subject->end(), subjectPublicKey.begin());
        grants.push_back({
            .subjectPublicKey = subjectPublicKey,
            .capability = static_cast<SessionCapability>(*capabilityValue),
        });
    }

    if (!validateCapabilityGrants(grants, controllerPublicKey, controlledPublicKey))
        return std::nullopt;
    return grants;
}

}  // namespace easydesk::protocol
