#include "ui/ui.h"
#include "ui/components/icons.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace Ui {
void enableChrome(QWidget *w, bool resizable, bool canMinimize) {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_SYSMENU;
    style &= ~WS_MAXIMIZEBOX;
    if (canMinimize)
        style |= WS_MINIMIZEBOX;
    else
        style &= ~WS_MINIMIZEBOX;
    if (resizable)
        style |= WS_THICKFRAME;
    else
        style &= ~WS_THICKFRAME;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    DWORD corner = 2;
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
    BOOL dark = isDark();
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
#else
    Q_UNUSED(w);
    Q_UNUSED(resizable);
    Q_UNUSED(canMinimize);
#endif
}

bool chromeEvent(QWidget *w, void *message, long *result, bool resizable) {
#ifdef Q_OS_WIN
    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCALCSIZE) {
        *result = 0;
        return true;
    }
    if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xfff0) == SC_MAXIMIZE) {
        *result = 0;
        return true;
    }
    if (msg->message == WM_NCHITTEST && resizable && !w->isFullScreen() && !w->isMaximized()) {
        RECT r;
        GetWindowRect(msg->hwnd, &r);
        int x = GET_X_LPARAM(msg->lParam), y = GET_Y_LPARAM(msg->lParam);
        int b = qRound(6 * w->devicePixelRatioF());
        bool left = x < r.left + b, right = x >= r.right - b, top = y < r.top + b, bottom = y >= r.bottom - b;
        if (top && left)
            *result = HTTOPLEFT;
        else if (top && right)
            *result = HTTOPRIGHT;
        else if (bottom && left)
            *result = HTBOTTOMLEFT;
        else if (bottom && right)
            *result = HTBOTTOMRIGHT;
        else if (left)
            *result = HTLEFT;
        else if (right)
            *result = HTRIGHT;
        else if (top)
            *result = HTTOP;
        else if (bottom)
            *result = HTBOTTOM;
        else
            return false;
        return true;
    }
#else
    Q_UNUSED(w);
    Q_UNUSED(message);
    Q_UNUSED(result);
    Q_UNUSED(resizable);
#endif
    return false;
}

void minimize(QWidget *w) {
#ifdef Q_OS_WIN
    ShowWindow(reinterpret_cast<HWND>(w->winId()), SW_MINIMIZE);
#else
    w->showMinimized();
#endif
}

TitleBar::TitleBar(QWidget *window, const QString &title, bool canMinimize) : QFrame(window) {
    setObjectName("titleBar");
    setFixedHeight(48);
    items = new QHBoxLayout(this);
    items->setContentsMargins(16, 0, 6, 0);
    items->setSpacing(10);
    auto *mark = new QLabel;
    mark->setPixmap(icon("logo").pixmap(22, 22));
    mark->setAttribute(Qt::WA_TransparentForMouseEvents);
    items->addWidget(mark);
    auto *brand = label(title, "brand");
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    items->addWidget(brand);
    items->addStretch();
    if (canMinimize) {
        auto *b = new QToolButton;
        b->setObjectName("min");
        b->setIcon(icon("min"));
        b->setAccessibleName(QStringLiteral("最小化"));
        b->setToolTip(QStringLiteral("最小化"));
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(42, 32);
        items->addWidget(b);
        connect(b, &QToolButton::clicked, window, [window] { minimize(window); });
    }
    auto *close = new QToolButton;
    close->setObjectName("close");
    close->setIcon(icon("close"));
    close->setAccessibleName(QStringLiteral("关闭"));
    close->setToolTip(QStringLiteral("关闭"));
    close->setFocusPolicy(Qt::NoFocus);
    close->setFixedSize(42, 32);
    items->addWidget(close);
    connect(close, &QToolButton::clicked, window, &QWidget::close);
}

void TitleBar::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && window()->windowHandle())
        window()->windowHandle()->startSystemMove();
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e) {
    e->accept();
}
} // namespace Ui
