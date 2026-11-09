#include "ui/input/desktop_input_capture.h"

#include "ui/input/video_input_mapping.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QSize physicalClientSize(QWidget *target) {
#ifdef Q_OS_WIN
    if (!target)
        return {};
    RECT rect{};
    if (!GetClientRect(reinterpret_cast<HWND>(target->winId()), &rect))
        return {};
    return QSize(rect.right - rect.left, rect.bottom - rect.top);
#else
    Q_UNUSED(target);
    return {};
#endif
}

bool mapButton(Qt::MouseButton button, RemoteMouseButton &output) {
    switch (button) {
    case Qt::LeftButton: output = RemoteMouseButton::Left; return true;
    case Qt::RightButton: output = RemoteMouseButton::Right; return true;
    case Qt::MiddleButton: output = RemoteMouseButton::Middle; return true;
    case Qt::BackButton: output = RemoteMouseButton::X1; return true;
    case Qt::ForwardButton: output = RemoteMouseButton::X2; return true;
    default: return false;
    }
}
}  // namespace

DesktopInputCapture::DesktopInputCapture(QWidget &target, SessionService &session,
                                         TargetSizeProvider targetSize, QObject *parent)
    : QObject(parent), m_target(&target), m_window(target.window()), m_session(session),
      m_targetSize(std::move(targetSize)), m_status(session.inputStatus()) {
    if (!m_targetSize)
        m_targetSize = [this] { return physicalClientSize(m_target); };
    target.setFocusPolicy(Qt::StrongFocus);
    target.setMouseTracking(true);
    target.setAttribute(Qt::WA_InputMethodEnabled, false);
    target.installEventFilter(this);
    if (m_window && m_window != m_target)
        m_window->installEventFilter(this);
    connect(&m_session, &SessionService::inputStatusChanged, this,
            [this](const SessionInputStatus &status) {
                if (m_status.ready && (!status.ready || status.displayGeneration != m_status.displayGeneration ||
                                       status.presentedTarget != m_status.presentedTarget))
                    releaseInput();
                m_status = status;
            });
}

void DesktopInputCapture::releaseInput() {
    m_session.releaseInput();
}

bool DesktopInputCapture::acceptsInput() const {
    return m_target && m_window && m_status.ready && m_target->hasFocus() &&
           m_window->isActiveWindow() && m_window->isVisible() && !m_window->isMinimized() &&
           m_targetSize && m_targetSize() == m_status.presentedTarget;
}

bool DesktopInputCapture::mapPoint(const QPointF &point, QPoint &normalized) const {
    if (!m_target || !m_targetSize)
        return false;
    const QSize physical = m_targetSize();
    return physical == m_status.presentedTarget &&
           mapVideoPointToRemote(point, QSizeF(m_target->size()), physical,
                                 m_status.presentedFrame, m_status.remoteRegion, normalized);
}

bool DesktopInputCapture::localShortcut(const QKeyEvent &event) const {
    const auto required = Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
    return (event.key() == Qt::Key_F11 || event.key() == Qt::Key_F12) &&
           (event.modifiers() & required) == required;
}

bool DesktopInputCapture::eventFilter(QObject *watched, QEvent *event) {
    if (!m_target || !m_window)
        return false;
    if ((watched == m_target && (event->type() == QEvent::FocusOut || event->type() == QEvent::Resize ||
                                 event->type() == QEvent::Hide || event->type() == QEvent::ScreenChangeInternal ||
                                 event->type() == QEvent::WinIdChange)) ||
        (watched == m_window && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide ||
                                 event->type() == QEvent::WindowStateChange || event->type() == QEvent::Move ||
                                 event->type() == QEvent::ScreenChangeInternal))) {
        releaseInput();
        return false;
    }
    if (watched != m_target)
        return false;
    if (m_status.ready && m_targetSize && m_targetSize() != m_status.presentedTarget) {
        releaseInput();
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress && m_status.ready && !m_target->hasFocus() &&
        m_window->isActiveWindow() && m_window->isVisible() && !m_window->isMinimized()) {
        QPoint ignored;
        if (mapPoint(static_cast<QMouseEvent *>(event)->localPos(), ignored))
            m_target->setFocus(Qt::MouseFocusReason);
    }
    if (!acceptsInput())
        return false;

    if (event->type() == QEvent::ShortcutOverride) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (!localShortcut(*key)) {
            event->accept();
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (localShortcut(*key))
            return false;
        if (event->type() == QEvent::KeyRelease && key->isAutoRepeat())
            return true;
        const quint32 scan = key->nativeScanCode();
        if (scan == 0 || scan > 0x1ffU)
            return true;
        const KeyInput input{static_cast<quint16>(scan & 0xffU), (scan & 0x100U) != 0,
                             event->type() == QEvent::KeyPress, key->isAutoRepeat()};
        if (input.scanCode != 0)
            (void)m_session.sendInput(InputStream::Reliable, input);
        return true;
    }
    if (event->type() == QEvent::MouseMove) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        QPoint position;
        if (mapPoint(mouse->localPos(), position))
            (void)m_session.sendInput(InputStream::Pointer,
                                      PointerMoveInput{static_cast<quint32>(position.x()),
                                                       static_cast<quint32>(position.y())});
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        RemoteMouseButton button;
        QPoint position;
        if (!mapButton(mouse->button(), button))
            return true;
        if (!mapPoint(mouse->localPos(), position)) {
            if (event->type() == QEvent::MouseButtonRelease)
                releaseInput();
            return true;
        }
        const bool pressed = event->type() != QEvent::MouseButtonRelease;
        (void)m_session.sendInput(InputStream::Reliable,
                                  PointerButtonInput{button, pressed, static_cast<quint32>(position.x()),
                                                     static_cast<quint32>(position.y())});
        return true;
    }
    if (event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        QPoint position;
        const QPoint delta = wheel->angleDelta();
        if ((!delta.isNull()) && mapPoint(wheel->position(), position))
            (void)m_session.sendInput(InputStream::Reliable,
                                      WheelInput{delta.x(), delta.y(), static_cast<quint32>(position.x()),
                                                 static_cast<quint32>(position.y())});
        return true;
    }
    return false;
}
