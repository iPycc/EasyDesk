#pragma once

#include <QFlags>
#include <QMetaType>

enum class SessionMode : quint8 {
    ViewOnly,
    Control,
};

enum class SessionCapability : quint8 {
    ViewDesktop = 0x01,
    ControlInput = 0x02,
    TransferFiles = 0x04,
};

Q_DECLARE_FLAGS(SessionCapabilities, SessionCapability)

SessionCapabilities capabilitiesForMode(SessionMode mode);
SessionCapabilities normalizedSessionCapabilities(SessionCapabilities capabilities);

struct SessionAccess {
    SessionCapabilities requested;
    SessionCapabilities granted;

    void request(SessionCapabilities capabilities);
    void grant(SessionCapabilities capabilities);
    void revoke(SessionCapability capability);
    void revokeAll();
    bool allows(SessionCapability capability) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SessionCapabilities)
Q_DECLARE_METATYPE(SessionMode)
Q_DECLARE_METATYPE(SessionCapabilities)
Q_DECLARE_METATYPE(SessionAccess)
