#pragma once

#include "features/sessions/input/input_command.h"
#include "input/input_ownership.h"

#include <QRect>
#include <functional>

enum class SendInputResult {
    Applied,
    InvalidEvent,
    DesktopUnavailable,
    InjectionRejected,
};

class SendInputExecutor final {
  public:
    struct Backend {
        std::function<bool()> desktopAvailable;
        std::function<quint32(void *, quint32)> send;
    };

    explicit SendInputExecutor(Backend backend = {});
    void setDisplayRect(const QRect &displayRect);
    SendInputResult apply(const InputEvent &event, InputStream stream);
    bool releaseAll();
    bool hasPendingRelease() const;

  private:
    QRect m_displayRect;
    InputOwnership m_ownership;
    Backend m_backend;

    bool interactiveDesktopAvailable() const;
    quint32 injectCount(void *inputs, int count) const;
    bool inject(void *inputs, int count) const;
    bool normalizedPosition(quint32 x, quint32 y, long &virtualX, long &virtualY) const;
    SendInputResult applyPointerMove(const PointerMoveInput &event);
    SendInputResult applyPointerButton(const PointerButtonInput &event);
    SendInputResult applyWheel(const WheelInput &event);
    SendInputResult applyKey(const KeyInput &event);
    SendInputResult applyText(const TextCommitInput &event);
};
