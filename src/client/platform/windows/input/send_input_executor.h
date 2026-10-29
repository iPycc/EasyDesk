#pragma once

#include "features/sessions/input/input_command.h"

#include <QRect>

#include <set>

enum class SendInputResult {
    Applied,
    InvalidEvent,
    DesktopUnavailable,
    InjectionRejected,
};

class SendInputExecutor final {
  public:
    void setDisplayRect(const QRect &displayRect);
    SendInputResult apply(const InputEvent &event, InputStream stream);
    void releaseAll();

  private:
    QRect m_displayRect;
    std::set<quint32> m_pressedKeys;
    std::set<RemoteMouseButton> m_pressedButtons;

    bool interactiveDesktopAvailable() const;
    bool normalizedPosition(quint32 x, quint32 y, long &virtualX, long &virtualY) const;
    SendInputResult applyPointerMove(const PointerMoveInput &event);
    SendInputResult applyPointerButton(const PointerButtonInput &event);
    SendInputResult applyWheel(const WheelInput &event);
    SendInputResult applyKey(const KeyInput &event);
    SendInputResult applyText(const TextCommitInput &event);
};
