#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

void MainWindow::buildTitleBar(QVBoxLayout *layout) {
    auto *bar = new Ui::TitleBar(this, "EasyDesk", true);
    auto *badge = text(QStringLiteral("预览"), "previewBadge");
    badge->setToolTip(QStringLiteral("本机信息来自系统；远程设备与画面为模拟数据"));
    bar->items->insertWidget(bar->items->count() - 2, badge);
    auto *theme = tool(QStringLiteral("切换浅色 / 深色"), "sun");
    theme->setObjectName("themeToggle");
    bar->items->insertWidget(bar->items->count() - 2, theme);
    connect(theme, &QToolButton::clicked, this, [this] {
        auto *choice = findChild<QComboBox *>("theme");
        choice->setCurrentIndex(Ui::isDark() ? 0 : 1);
    });
    auto *about = tool(QStringLiteral("关于 EasyDesk"), "info");
    about->setObjectName("aboutButton");
    bar->items->insertWidget(bar->items->count() - 2, about);
    connect(about, &QToolButton::clicked, this, &MainWindow::showAbout);
    layout->addWidget(bar);
}

QWidget *MainWindow::buildSidebar() {
    auto *side = new QWidget;
    side->setObjectName("sidebar");
    side->setFixedWidth(240);
    auto *l = column(side, 4, 12);
    l->setContentsMargins(4, 0, 4, 12);
    auto *local = new QWidget;
    local->setObjectName("localDevice");
    auto *ll = column(local, 12, 8);
    auto *name = row(10);
    auto *mark = new QLabel;
    mark->setObjectName("localComputerPlatform");
    mark->setFixedSize(26, 26);
    name->addWidget(mark);
    auto *localName = new Ui::ElidedLabel(QString());
    localName->setProperty("role", "localName");
    localName->setObjectName("localComputerName");
    auto refreshLocal = [this, localName, mark] {
        const auto info = m_local.information();
        localName->setText(info.displayName);
        localName->setToolTip(info.computerName);
        mark->setPixmap(icon(platformIcon(info.operatingSystem), Ui::accent()).pixmap(26, 26));
        mark->setAccessibleName(info.operatingSystem);
        mark->setToolTip(info.operatingSystem);
    };
    refreshLocal();
    connect(&m_local, &LocalSystemService::changed, local, refreshLocal);
    name->addWidget(localName, 1);
    ll->addLayout(name);
    auto *id = row(4);
    auto *localId = text(spacedId(m_sessions.localIdentity().id), "localId");
    id->addWidget(localId);
    connect(&m_sessions, &SessionService::identityChanged, local, [this, localId] {
        const auto identity = m_sessions.localIdentity();
        localId->setText(spacedId(identity.id));
    });
    id->addStretch();
    auto *copy = tool(QStringLiteral("复制本机 ID"), "copy");
    copy->setFixedSize(26, 26);
    id->addWidget(copy);
    ll->addLayout(id);
    l->addWidget(local);
    local->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(local, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &) { showDeviceDetails("local-machine"); });
    connect(copy, &QToolButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_sessions.localIdentity().id);
        notify(QStringLiteral("已复制设备 ID"));
    });
    m_nav = new QWidget;
    m_nav->setObjectName("navigation");
    m_nav->setAccessibleName(QStringLiteral("主导航"));
    m_navLayout = column(m_nav, 0, 4);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_nav);
    l->addWidget(scroll, 1);
    m_navIndicator = new Ui::SelectionIndicator(m_nav);
    m_navIndicator->setObjectName("navigationIndicator");
    auto *settings = new Ui::NavButton(QStringLiteral("设置"), "settings");
    settings->setObjectName("settingsNav");
    connect(settings, &QPushButton::clicked, this, [this] { navigate("settings"); });
    l->addWidget(settings);
    return side;
}

void MainWindow::rebuildNav() {
    while (auto *item = m_navLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->hide();
            w->setObjectName(QString());
            w->deleteLater();
        }
        delete item;
    }
    m_routes.clear();
    auto section = [this](const QString &title, const QString &glyph, bool expanded,
                          const std::function<void()> &toggle) {
        auto *b = new Ui::NavButton(title, glyph);
        b->setCheckable(false);
        b->setProperty("kind", "navGroup");
        b->setIconSize(QSize(22, 22));
        b->setProperty("expanded", expanded);
        b->setObjectName(glyph == "devices" ? "myDevicesGroup" : "remoteGroup");
        connect(b, &QPushButton::clicked, this, toggle);
        m_navLayout->addWidget(b);
    };
    auto item = [this](const QString &title, const QString &glyph, const QString &route) {
        auto *b = new Ui::NavButton(title, glyph);
        b->setProperty("depth", 1);
        b->setProperty("sharedIndicator", true);
        b->setObjectName("nav-" + route);
        m_routes.insert(route, b);
        b->setChecked(route == (m_route == "device" ? "device:" + m_currentId : m_route));
        m_navLayout->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, route] { navigate(route); });
    };
    section(QStringLiteral("我的设备"), "devices", m_myExpanded, [this] {
        m_myExpanded = !m_myExpanded;
        rebuildNav();
    });
    if (m_myExpanded) {
        for (int n = 0; n < m_devices.devices().size(); ++n) {
            const auto &d = m_devices.devices()[n];
            item(d.name, platformIcon(d.system), "device:" + d.id);
        }
        item(QStringLiteral("全部设备"), "grid", "all");
    }
    section(QStringLiteral("远程控制"), "remote", m_remoteExpanded, [this] {
        m_remoteExpanded = !m_remoteExpanded;
        rebuildNav();
    });
    if (m_remoteExpanded) {
        item(QStringLiteral("开始远程"), "play", "start");
        item(QStringLiteral("收藏设备"), "star", "favorites");
    }
    m_navLayout->addStretch();
    QTimer::singleShot(0, this, [this] { updateNavIndicator(false); });
}

void MainWindow::updateNavIndicator(bool animate) {
    if (!m_navIndicator)
        return;
    const auto key = m_route == "device" ? "device:" + m_currentId : m_route;
    auto *target = m_routes.value(key, nullptr);
    if (!target || !target->isVisible()) {
        m_navIndicator->slideTo({});
        return;
    }
    m_navIndicator->slideTo(QRect(0, target->y() + (target->height() - 18) / 2, 3, 18), animate);
}
