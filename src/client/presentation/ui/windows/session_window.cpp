#include "ui/ui.h"
#include "ui/components/icons.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace Ui {
SessionWindow::SessionWindow(QWidget *anchor, const QString &title)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint |
                           Qt::WindowCloseButtonHint),
      m_anchor(anchor) {
    setObjectName("sessionWindow");
    setWindowTitle(title);
    setWindowIcon(icon("logo"));
    setAttribute(Qt::WA_DeleteOnClose);
    m_frame = new QVBoxLayout(this);
    m_frame->setContentsMargins(1, 1, 1, 1);
    m_frame->setSpacing(0);
    m_titleBar = new TitleBar(this, title, true);
    m_frame->addWidget(m_titleBar);
    auto *surface = new QWidget;
    surface->setObjectName("sessionSurface");
    body = new QVBoxLayout(surface);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    m_frame->addWidget(surface, 1);
    setProperty("fullscreen", false);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    style = (style | WS_EX_APPWINDOW) & ~WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
#endif
    enableChrome(this, true, true);
}

bool SessionWindow::nativeEvent(const QByteArray &type, void *message, long *result) {
    if (chromeEvent(this, message, result, true))
        return true;
    return QWidget::nativeEvent(type, message, result);
}

void SessionWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_centered || isFullScreen())
        return;
    m_centered = true;
    QScreen *screen =
        m_anchor ? QGuiApplication::screenAt(m_anchor->mapToGlobal(m_anchor->rect().center())) : nullptr;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect available = screen->availableGeometry();
    QPoint center = m_anchor ? m_anchor->mapToGlobal(m_anchor->rect().center()) : available.center();
    QPoint pos = center - QPoint(width() / 2, height() / 2);
    move(qBound(available.left(), pos.x(), qMax(available.left(), available.right() - width() + 1)),
         qBound(available.top(), pos.y(), qMax(available.top(), available.bottom() - height() + 1)));
}

void SessionWindow::updatePresentation(bool fullscreen) {
    if (m_fullscreen == fullscreen)
        return;
    m_fullscreen = fullscreen;
    setProperty("fullscreen", fullscreen);
    m_titleBar->setVisible(!fullscreen);
    m_frame->setContentsMargins(fullscreen ? 0 : 1, fullscreen ? 0 : 1, fullscreen ? 0 : 1,
                                fullscreen ? 0 : 1);
    for (auto *surface :
         body->parentWidget()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
        surface->setProperty("fullscreen", fullscreen);
        surface->update();
    }
    style()->unpolish(this);
    style()->polish(this);
    m_frame->activate();
    emit fullscreenChanged(fullscreen);
}

void SessionWindow::setFullscreen(bool enabled) {
    if (enabled == isFullScreen())
        return;
    if (enabled) {
        m_windowedGeometry = geometry();
        updatePresentation(true);
        showFullScreen();
    } else {
        showNormal();
        updatePresentation(false);
        if (m_windowedGeometry.isValid())
            setGeometry(m_windowedGeometry);
    }
}

void SessionWindow::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updatePresentation(isFullScreen());
}

void SessionWindow::closeEvent(QCloseEvent *event) {
    if (auto *dialog = activeDialog(); dialog && dialog->host() == this) {
        dialog->reject();
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
    if (event->isAccepted())
        emit closed();
}
} // namespace Ui
