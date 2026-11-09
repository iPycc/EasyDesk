#pragma once

#include "features/sessions/input/input_command.h"
#include "input/input_ownership.h"

#include <QRect>

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
    InputOwnership m_ownership;

    bool interactiveDesktopAvailable() const;
    bool normalizedPosition(quint32 x, quint32 y, long &virtualX, long &virtualY) const;
    SendInputResult applyPointerMove(const PointerMoveInput &event);
    SendInputResult applyPointerButton(const PointerButtonInput &event);
    SendInputResult applyWheel(const WheelInput &event);
    SendInputResult applyKey(const KeyInput &event);
    SendInputResult applyText(const TextCommitInput &event);
};
