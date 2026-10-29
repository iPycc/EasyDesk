#include "ui/components/desktop_view.h"

#include "ui/components/icons.h"
#include <QPainter>
#include <QVariant>

DesktopView::DesktopView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setMinimumHeight(170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("远程桌面视频区域"));
}

void DesktopView::setDevice(const QString &, bool online) {
    m_online = online;
    update();
}

void DesktopView::setScreen(int screen) {
    m_screen = screen;
    update();
}

void DesktopView::setSession(bool session) {
    m_session = session;
    update();
}

void DesktopView::setOriginalSize(bool original) {
    m_original = original;
    update();
}

void DesktopView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), m_session || property("fullscreen").toBool() ? QColor(Qt::black)
                                                                         : QColor("#e8ebf0"));
    const QString state = !m_online ? QStringLiteral("设备离线") : QStringLiteral("等待画面");
    const QColor foreground = m_session ? QColor("#d5d9df") : QColor("#71849b");
    icon("monitor", foreground, 42)
        .paint(&painter, QRect(width() / 2 - 21, height() / 2 - 45, 42, 42));
    painter.setPen(foreground);
    QFont labelFont = font();
    labelFont.setPixelSize(14);
    painter.setFont(labelFont);
    painter.drawText(QRect(0, height() / 2 + 8, width(), 28), Qt::AlignCenter, state);
}
