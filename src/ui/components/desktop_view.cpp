#include "ui/components/desktop_view.h"
#include "ui/components/icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QVariant>

DesktopView::DesktopView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setMinimumHeight(170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("模拟远程桌面视频窗"));
    connect(&m_timer, &QTimer::timeout, this, [this] {
        ++m_tick;
        if (isVisible())
            update();
    });
    m_timer.start(1000);
}
void DesktopView::setDevice(const QString &name, bool online) {
    m_name = name;
    m_online = online;
    update();
}
void DesktopView::setScreen(int s) {
    m_screen = s;
    update();
}
void DesktopView::setSession(bool b) {
    m_session = b;
    update();
}
void DesktopView::setOriginalSize(bool b) {
    m_original = b;
    update();
}
void DesktopView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), property("fullscreen").toBool() ? QColor(Qt::black) : QColor("#e8ebf0"));
    QRectF canvas = rect();
    const double ratio = 16.0 / 9;
    if (width() / double(height()) > ratio) {
        canvas.setWidth(height() * ratio);
        canvas.moveCenter(rect().center());
    } else {
        canvas.setHeight(width() / ratio);
        canvas.moveCenter(rect().center());
    }
    if (m_original) {
        canvas = QRectF(0, 0, 1920, 1080);
        canvas.moveCenter(rect().center());
    }
    p.save();
    p.setClipRect(canvas);
    p.translate(canvas.topLeft());
    p.scale(canvas.width() / 960, canvas.height() / 540);
    p.fillRect(QRectF(0, 0, 960, 540), QColor(m_screen == 1 ? "#edf0f5" : "#e9efed"));
    // Restrained geometric wallpaper, painted directly with QPainter.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#e1e6ee"));
    p.drawEllipse(QRectF(405, -190, 770, 770));
    p.setBrush(QColor("#d3ddea"));
    p.drawEllipse(QRectF(510, -110, 590, 590));
    p.setBrush(QColor("#c3d1e5"));
    p.drawEllipse(QRectF(605, -10, 410, 410));
    p.setBrush(QColor("#edf0f5"));
    p.drawEllipse(QRectF(664, 41, 288, 288));
    p.setPen(QColor("#525b68"));
    QFont small = font();
    small.setPixelSize(14);
    p.setFont(small);
    const QStringList names = {QStringLiteral("此电脑"), QStringLiteral("项目文件"),
                               QStringLiteral("回收站")};
    for (int i = 0; i < 3; ++i) {
        auto r = QRect(37, 31 + i * 90, 30, 30);
        icon(i == 1 ? "folder" : "monitor", QColor("#3478ed"), 30).paint(&p, r);
        p.drawText(QRectF(15, 66 + i * 90, 76, 24), Qt::AlignCenter, names[i]);
    }
    // A synthetic Explorer window; no local or remote files are read here.
    const QRectF explorer(173, 80, 490, 330);
    p.setBrush(QColor(70, 105, 145, 16));
    p.drawRoundedRect(explorer.translated(0, 6), 8, 8);
    p.setBrush(QColor("#ffffff"));
    p.drawRoundedRect(explorer, 7, 7);
    p.setPen(QColor("#303640"));
    p.drawText(QRectF(195, 92, 350, 30), Qt::AlignVCenter, QStringLiteral("项目文件"));
    p.setPen(QPen(QColor("#90a1b5"), 1));
    p.drawLine(590, 105, 598, 105);
    p.drawRect(QRectF(615, 101, 8, 8));
    p.drawLine(641, 101, 649, 109);
    p.drawLine(649, 101, 641, 109);
    p.fillRect(QRectF(174, 129, 488, 38), QColor("#f5f6f8"));
    p.setPen(QColor("#697383"));
    p.drawText(QRectF(196, 130, 440, 36), Qt::AlignVCenter,
               QStringLiteral("此电脑  /  工作空间  /  项目文件"));
    p.fillRect(QRectF(174, 167, 118, 242), QColor("#f7f7f8"));
    p.setPen(QColor("#606875"));
    for (int i = 0; i < 4; ++i)
        p.drawText(QRectF(192, 184 + i * 34, 90, 30), Qt::AlignVCenter,
                   QStringList{QStringLiteral("快速访问"), QStringLiteral("桌面"), QStringLiteral("文档"),
                               QStringLiteral("下载")}[i]);
    QStringList folders = {QStringLiteral("设计素材"), QStringLiteral("工作文档"),
                           QStringLiteral("项目归档")};
    for (int i = 0; i < 3; ++i) {
        int x = 324 + i * 108;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3478ed"));
        p.drawRoundedRect(QRectF(x, 211, 23, 10), 2, 2);
        p.setBrush(QColor("#77a6f4"));
        p.drawRoundedRect(QRectF(x, 218, 54, 39), 4, 4);
        p.setPen(QColor("#505a68"));
        p.drawText(QRectF(x - 10, 268, 76, 25), Qt::AlignCenter, folders[i]);
    }
    p.setPen(QColor("#93a1b2"));
    p.drawText(QRectF(314, 361, 320, 24), Qt::AlignLeft, QStringLiteral("3 个文件夹"));
    p.fillRect(QRectF(0, 502, 960, 38), QColor("#f6f7f9"));
    for (int i = 0; i < 4; ++i)
        icon(QStringList{"grid", "search", "folder", "monitor"}[i], QColor("#5a83b5"), 19)
            .paint(&p, QRect(388 + i * 44, 512, 19, 19));
    p.setPen(QColor("#647c96"));
    p.drawText(QRectF(816, 509, 117, 23), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("09:%1").arg(41 + (m_tick / 60) % 19));
    p.restore();
    if (!m_online) {
        p.fillRect(rect(), QColor(244, 248, 253, 235));
        icon("monitor", QColor("#9caec4"), 46).paint(&p, QRect(width() / 2 - 23, height() / 2 - 48, 46, 46));
        p.setPen(QColor("#71849b"));
        QFont f = font();
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(QRect(0, height() / 2 + 9, width(), 28), Qt::AlignCenter, QStringLiteral("设备离线"));
    }
}
