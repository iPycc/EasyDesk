#include "features/sessions/model/session_access.h"

SessionCapabilities capabilitiesForMode(SessionMode mode) {
    SessionCapabilities capabilities(SessionCapability::ViewDesktop);
    if (mode == SessionMode::Control)
        capabilities |= SessionCapability::ControlInput;
    return capabilities;
}

SessionCapabilities normalizedSessionCapabilities(SessionCapabilities capabilities) {
    if (capabilities.testFlag(SessionCapability::ControlInput))
        capabilities |= SessionCapability::ViewDesktop;
    return capabilities;
}

void SessionAccess::request(SessionCapabilities capabilities) {
    requested = normalizedSessionCapabilities(capabilities);
    granted &= requested;
    granted = normalizedSessionCapabilities(granted);
}

void SessionAccess::grant(SessionCapabilities capabilities) {
    granted = normalizedSessionCapabilities(capabilities & requested);
}

void SessionAccess::revoke(SessionCapability capability) {
    granted &= ~SessionCapabilities(capability);
    if (capability == SessionCapability::ViewDesktop)
        granted &= ~SessionCapabilities(SessionCapability::ControlInput);
}

void SessionAccess::revokeAll() {
    granted = {};
}

bool SessionAccess::allows(SessionCapability capability) const {
    return granted.testFlag(capability);
}
