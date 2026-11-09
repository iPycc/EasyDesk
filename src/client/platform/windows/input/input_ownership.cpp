#include "input/input_ownership.h"

namespace {
quint32 keyId(const KeyInput &event) {
    return static_cast<quint32>(event.scanCode) | (event.extended ? 0x10000U : 0U);
}
}

bool InputOwnership::permitsKey(const KeyInput &event) const {
    if (event.scanCode == 0 || (event.repeat && !event.pressed))
        return false;
    const bool held = m_keys.count(keyId(event)) != 0;
    if (!event.pressed)
        return held;
    return event.repeat ? held : !held;
}

bool InputOwnership::permitsButton(const PointerButtonInput &event) const {
    const bool held = m_buttons.count(event.button) != 0;
    return event.pressed ? !held : held;
}

void InputOwnership::recordKey(const KeyInput &event) {
    if (event.pressed)
        m_keys.insert(keyId(event));
    else
        m_keys.erase(keyId(event));
}

void InputOwnership::recordButton(const PointerButtonInput &event) {
    if (event.pressed)
        m_buttons.insert(event.button);
    else
        m_buttons.erase(event.button);
}

const std::set<quint32> &InputOwnership::keys() const { return m_keys; }
const std::set<RemoteMouseButton> &InputOwnership::buttons() const { return m_buttons; }

void InputOwnership::acknowledgeKeyRelease(quint32 key) { m_keys.erase(key); }
void InputOwnership::acknowledgeButtonRelease(RemoteMouseButton button) { m_buttons.erase(button); }
bool InputOwnership::empty() const { return m_keys.empty() && m_buttons.empty(); }

void InputOwnership::clear() {
    m_keys.clear();
    m_buttons.clear();
}
