#pragma once

#include "features/sessions/input/input_command.h"

#include <set>

class InputOwnership final {
  public:
    bool permitsKey(const KeyInput &event) const;
    bool permitsButton(const PointerButtonInput &event) const;
    void recordKey(const KeyInput &event);
    void recordButton(const PointerButtonInput &event);
    const std::set<quint32> &keys() const;
    const std::set<RemoteMouseButton> &buttons() const;
    void clear();

  private:
    std::set<quint32> m_keys;
    std::set<RemoteMouseButton> m_buttons;
};
