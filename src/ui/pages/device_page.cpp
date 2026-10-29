#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/desktop_view.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

QWidget *MainWindow::buildDevicePage() {
    QVBoxLayout *l;
    auto *p = page(&l);
    l->setSpacing(14);
    auto *h = row(10);
    m_deviceTitle = text("", "h1");
    h->addWidget(m_deviceTitle);
    m_onlineBadge = text("", "onlineBadge");
    m_onlineBadge->setFixedHeight(24);
    h->addWidget(m_onlineBadge);
    h->addStretch();
    m_favorite = button("", "star", "icon");
    m_favorite->setFixedSize(32, 32);
    m_favorite->setToolTip(QStringLiteral("收藏 / 取消收藏"));
    m_favorite->setAccessibleName(QStringLiteral("收藏 / 取消收藏"));
    h->addWidget(m_favorite);
    auto *more = tool(QStringLiteral("设备选项"), "more");
    more->setObjectName("deviceMore");
    h->addWidget(more);
    l->addLayout(h);
    connect(m_favorite, &QPushButton::clicked, this, [this] { toggleFavorite(m_currentId); });
    auto *menu = new Ui::Menu(more);
    menu->setObjectName("deviceMenu");
    menu->addAction(icon("file"), QStringLiteral("重命名设备"), this, &MainWindow::renameDevice);
    menu->addAction(icon("copy"), QStringLiteral("复制设备 ID"), this, [this] {
        QApplication::clipboard()->setText(m_currentId);
        notify(QStringLiteral("已复制设备 ID"));
    });
    menu->addSeparator();
    menu->addAction(icon("delete"), QStringLiteral("移除设备"), this, &MainWindow::removeDevice);
    more->setMenu(menu);
    more->setPopupMode(QToolButton::InstantPopup);
    auto *previewFrame = new QFrame;
    previewFrame->setObjectName("previewFrame");
    auto *pl = column(previewFrame, 1, 0);
    m_preview = new DesktopView;
    m_preview->setObjectName("desktopPreview");
    m_preview->setMinimumHeight(214);
    pl->addWidget(m_preview, 1);
    auto *toolbar = new QToolBar;
    toolbar->setObjectName("desktopToolbar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(17, 17));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setFixedHeight(48);
    auto addTool = [toolbar](const QString &label, const QString &ico) {
        auto *b = new QToolButton;
        b->setText(label);
        b->setIcon(icon(ico));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(32);
        toolbar->addWidget(b);
        return b;
    };
    auto *files = addTool(QStringLiteral("文件传输"), "folder");
    connect(files, &QToolButton::clicked, this, &MainWindow::openTransfers);
    auto *quality = addTool(QStringLiteral("画质"), "quality");
    auto *qm = new Ui::Menu(quality);
    auto *qgroup = new QActionGroup(qm);
    qgroup->setExclusive(true);
    for (const QString &s :
         QStringList{QStringLiteral("清晰"), QStringLiteral("流畅"), QStringLiteral("节流")}) {
        auto *a = qm->addAction(s);
        a->setCheckable(true);
        qgroup->addAction(a);
        if (qgroup->actions().size() == 1)
            a->setChecked(true);
        connect(a, &QAction::triggered, this, [this, s] { notify(QStringLiteral("画质已切换：") + s); });
    }
    quality->setMenu(qm);
    quality->setPopupMode(QToolButton::InstantPopup);
    auto *screen = addTool(QStringLiteral("屏幕 1"), "monitor");
    auto *sm = new Ui::Menu(screen);
    auto *sg = new QActionGroup(sm);
    sg->setExclusive(true);
    for (int n : {1, 2}) {
        auto *a = sm->addAction(QStringLiteral("屏幕 %1").arg(n));
        a->setCheckable(true);
        a->setChecked(n == 1);
        sg->addAction(a);
        connect(a, &QAction::triggered, this, [this, n, screen] {
            m_preview->setScreen(n);
            screen->setText(QStringLiteral("屏幕 %1").arg(n));
        });
    }
    screen->setMenu(sm);
    screen->setPopupMode(QToolButton::InstantPopup);
    auto *full = addTool("", "fullscreen");
    full->setToolTip(QStringLiteral("打开演示桌面窗口"));
    full->setAccessibleName(QStringLiteral("打开演示桌面窗口"));
    connect(full, &QToolButton::clicked, this, [this] { startSession(m_currentId); });
    auto *space = new QWidget;
    space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(space);
    m_connect = button(QStringLiteral("开始远程"), "arrow", "primary");
    m_connect->setObjectName("deviceConnect");
    m_connect->setMinimumHeight(32);
    m_connect->setMinimumWidth(104);
    toolbar->addWidget(m_connect);
    auto *pad = new QWidget;
    pad->setFixedWidth(8);
    toolbar->addWidget(pad);
    connect(m_connect, &QPushButton::clicked, this, [this] { startSession(m_currentId); });
    pl->addWidget(toolbar);
    l->addWidget(previewFrame, 1);
    l->addWidget(text(QStringLiteral("快捷启动"), "h3"));
    m_quickHost = new QWidget;
    m_quickHost->setObjectName("quickLaunchHost");
    m_quickHost->setMinimumHeight(114);
    column(m_quickHost, 0, 8);
    l->addWidget(m_quickHost);
    return p;
}
