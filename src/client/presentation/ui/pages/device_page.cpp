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
    return p;
}
