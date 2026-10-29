#include "input/send_input_executor.h"

#include <QVarLengthArray>

#include <windows.h>

#include <algorithm>
#include <array>
#include <type_traits>

namespace {
constexpr quint32 kMaximumNormalizedCoordinate = 65535;
constexpr int kMaximumTextCodeUnits = 256;
constexpr ULONG_PTR kEasyDeskInputMarker = static_cast<ULONG_PTR>(0x454153594445534BULL);

INPUT mouseInput(DWORD flags, long x = 0, long y = 0, DWORD data = 0) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.mouseData = data;
    input.mi.dwFlags = flags;
    input.mi.dwExtraInfo = kEasyDeskInputMarker;
    return input;
}

INPUT scanCodeInput(quint16 scanCode, bool extended, bool pressed) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = scanCode;
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (extended)
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!pressed)
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = kEasyDeskInputMarker;
    return input;
}

INPUT unicodeInput(ushort codeUnit, bool pressed) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = codeUnit;
    input.ki.dwFlags = KEYEVENTF_UNICODE | (pressed ? 0 : KEYEVENTF_KEYUP);
    input.ki.dwExtraInfo = kEasyDeskInputMarker;
    return input;
}

bool inject(INPUT *inputs, int count) {
    if (count <= 0)
        return true;
    return SendInput(static_cast<UINT>(count), inputs, sizeof(INPUT)) == static_cast<UINT>(count);
}

DWORD buttonFlag(RemoteMouseButton button, bool pressed) {
    switch (button) {
    case RemoteMouseButton::Left:
        return pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case RemoteMouseButton::Right:
        return pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case RemoteMouseButton::Middle:
        return pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    case RemoteMouseButton::X1:
    case RemoteMouseButton::X2:
        return pressed ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    }
    return 0;
}

DWORD buttonData(RemoteMouseButton button) {
    if (button == RemoteMouseButton::X1)
        return XBUTTON1;
    if (button == RemoteMouseButton::X2)
        return XBUTTON2;
    return 0;
}

bool desktopName(HDESK desktop, std::array<wchar_t, 128> &name) {
    DWORD requiredBytes = 0;
    return GetUserObjectInformationW(desktop, UOI_NAME, name.data(),
                                     static_cast<DWORD>(name.size() * sizeof(wchar_t)),
                                     &requiredBytes) != FALSE;
}
} // namespace

void SendInputExecutor::setDisplayRect(const QRect &displayRect) {
    m_displayRect = displayRect;
}

SendInputResult SendInputExecutor::apply(const InputEvent &event, InputStream stream) {
    if (!interactiveDesktopAvailable())
        return SendInputResult::DesktopUnavailable;
    if (stream == InputStream::Pointer && !std::holds_alternative<PointerMoveInput>(event))
        return SendInputResult::InvalidEvent;

    return std::visit(
        [this](const auto &value) -> SendInputResult {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, PointerMoveInput>)
                return applyPointerMove(value);
            if constexpr (std::is_same_v<Event, PointerButtonInput>)
                return applyPointerButton(value);
            if constexpr (std::is_same_v<Event, WheelInput>)
                return applyWheel(value);
            if constexpr (std::is_same_v<Event, KeyInput>)
                return applyKey(value);
            if constexpr (std::is_same_v<Event, TextCommitInput>)
                return applyText(value);
            if constexpr (std::is_same_v<Event, ReleaseAllInput>) {
                releaseAll();
                return SendInputResult::Applied;
            }
            return SendInputResult::InvalidEvent;
        },
        event);
}

bool SendInputExecutor::interactiveDesktopAvailable() const {
    HDESK inputDesktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!inputDesktop)
        return false;
    const HDESK threadDesktop = GetThreadDesktop(GetCurrentThreadId());
    std::array<wchar_t, 128> inputName{};
    std::array<wchar_t, 128> threadName{};
    const bool available = desktopName(inputDesktop, inputName) &&
                           desktopName(threadDesktop, threadName) && inputName == threadName;
    CloseDesktop(inputDesktop);
    return available;
}

bool SendInputExecutor::normalizedPosition(quint32 x, quint32 y, long &virtualX,
                                           long &virtualY) const {
    if (x > kMaximumNormalizedCoordinate || y > kMaximumNormalizedCoordinate ||
        m_displayRect.width() <= 0 || m_displayRect.height() <= 0)
        return false;

    const int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (virtualWidth <= 0 || virtualHeight <= 0)
        return false;

    const qint64 pixelX = m_displayRect.left() +
                          (static_cast<qint64>(m_displayRect.width() - 1) * x) /
                              kMaximumNormalizedCoordinate;
    const qint64 pixelY = m_displayRect.top() +
                          (static_cast<qint64>(m_displayRect.height() - 1) * y) /
                              kMaximumNormalizedCoordinate;
    const qint64 clampedX = std::clamp<qint64>(pixelX, virtualLeft,
                                               virtualLeft + virtualWidth - 1);
    const qint64 clampedY = std::clamp<qint64>(pixelY, virtualTop,
                                               virtualTop + virtualHeight - 1);
    virtualX = virtualWidth == 1
                   ? 0
                   : static_cast<long>(((clampedX - virtualLeft) * kMaximumNormalizedCoordinate) /
                                       (virtualWidth - 1));
    virtualY = virtualHeight == 1
                   ? 0
                   : static_cast<long>(((clampedY - virtualTop) * kMaximumNormalizedCoordinate) /
                                       (virtualHeight - 1));
    return true;
}

SendInputResult SendInputExecutor::applyPointerMove(const PointerMoveInput &event) {
    long x = 0;
    long y = 0;
    if (!normalizedPosition(event.normalizedX, event.normalizedY, x, y))
        return SendInputResult::InvalidEvent;
    INPUT input = mouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                             x, y);
    return inject(&input, 1) ? SendInputResult::Applied : SendInputResult::InjectionRejected;
}

SendInputResult SendInputExecutor::applyPointerButton(const PointerButtonInput &event) {
    long x = 0;
    long y = 0;
    const DWORD flag = buttonFlag(event.button, event.pressed);
    if (flag == 0 || !normalizedPosition(event.normalizedX, event.normalizedY, x, y))
        return SendInputResult::InvalidEvent;
    std::array<INPUT, 2> inputs{
        mouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, x, y),
        mouseInput(flag, 0, 0, buttonData(event.button)),
    };
    if (!inject(inputs.data(), static_cast<int>(inputs.size())))
        return SendInputResult::InjectionRejected;
    if (event.pressed)
        m_pressedButtons.insert(event.button);
    else
        m_pressedButtons.erase(event.button);
    return SendInputResult::Applied;
}

SendInputResult SendInputExecutor::applyWheel(const WheelInput &event) {
    long x = 0;
    long y = 0;
    if (!normalizedPosition(event.normalizedX, event.normalizedY, x, y) ||
        (event.horizontalDelta == 0 && event.verticalDelta == 0))
        return SendInputResult::InvalidEvent;
    std::array<INPUT, 3> inputs{};
    int count = 0;
    inputs[count++] =
        mouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, x, y);
    if (event.verticalDelta != 0)
        inputs[count++] = mouseInput(MOUSEEVENTF_WHEEL, 0, 0,
                                     static_cast<DWORD>(event.verticalDelta));
    if (event.horizontalDelta != 0)
        inputs[count++] = mouseInput(MOUSEEVENTF_HWHEEL, 0, 0,
                                     static_cast<DWORD>(event.horizontalDelta));
    return inject(inputs.data(), count) ? SendInputResult::Applied
                                        : SendInputResult::InjectionRejected;
}

SendInputResult SendInputExecutor::applyKey(const KeyInput &event) {
    if (event.scanCode == 0)
        return SendInputResult::InvalidEvent;
    INPUT input = scanCodeInput(event.scanCode, event.extended, event.pressed);
    if (!inject(&input, 1))
        return SendInputResult::InjectionRejected;
    const quint32 key = static_cast<quint32>(event.scanCode) | (event.extended ? 0x10000U : 0U);
    if (event.pressed)
        m_pressedKeys.insert(key);
    else
        m_pressedKeys.erase(key);
    return SendInputResult::Applied;
}

SendInputResult SendInputExecutor::applyText(const TextCommitInput &event) {
    if (event.text.isEmpty() || event.text.size() > kMaximumTextCodeUnits)
        return SendInputResult::InvalidEvent;
    QVarLengthArray<INPUT, 64> inputs(event.text.size() * 2);
    for (int index = 0; index < event.text.size(); ++index) {
        const ushort codeUnit = event.text.at(index).unicode();
        inputs[index * 2] = unicodeInput(codeUnit, true);
        inputs[index * 2 + 1] = unicodeInput(codeUnit, false);
    }
    return inject(inputs.data(), inputs.size()) ? SendInputResult::Applied
                                                : SendInputResult::InjectionRejected;
}

void SendInputExecutor::releaseAll() {
    QVarLengthArray<INPUT, 32> inputs;
    inputs.reserve(static_cast<int>(m_pressedKeys.size() + m_pressedButtons.size()));
    for (quint32 key : m_pressedKeys)
        inputs.push_back(scanCodeInput(static_cast<quint16>(key & 0xFFFFU),
                                       (key & 0x10000U) != 0, false));
    for (RemoteMouseButton button : m_pressedButtons)
        inputs.push_back(mouseInput(buttonFlag(button, false), 0, 0, buttonData(button)));
    inject(inputs.data(), inputs.size());
    m_pressedKeys.clear();
    m_pressedButtons.clear();
}
