#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include "ui/components/desktop_view.h"
#include <QtWidgets>
using namespace Ui::Layout;

MainWindow::MainWindow(DeviceService &devices, SessionService &sessions, TransferService &transfers,
                       LocalSystemService &local, QSettings &settings, QWidget *parent)
    : QWidget(parent), m_devices(devices), m_sessions(sessions), m_transfers(transfers), m_local(local) {
    m_settings = &settings;
    setObjectName("mainWindow");
    setWindowTitle(QStringLiteral("EasyDesk · 工作台"));
    setWindowIcon(icon("logo", QColor("#3478ed"), 64));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint |
                   Qt::WindowCloseButtonHint);
    resize(920, 680);
    setMinimumSize(860, 620);
    auto *outer = column(this, 0, 0);
    auto *shell = new QFrame;
    shell->setObjectName("shell");
    outer->addWidget(shell);
    auto *layout = column(shell, 1, 0);
    buildTitleBar(layout);
    auto *body = row(0);
    body->addWidget(buildSidebar());
    auto *contentHost = new QWidget;
    auto *hostLayout = column(contentHost, 0, 0);
    hostLayout->setContentsMargins(0, 0, 12, 12);
    auto *content = new QFrame;
    content->setObjectName("contentPanel");
    auto *contentLayout = column(content, 1, 0);
    hostLayout->addWidget(content, 1);
    m_pages = new QStackedWidget;
    m_pages->setObjectName("pages");
    m_pages->addWidget(buildDevicePage());
    m_pages->addWidget(buildListPage());
    m_pages->addWidget(buildStartPage());
    m_pages->addWidget(buildSettingsPage());
    contentLayout->addWidget(m_pages, 1);
    m_status = new Ui::Toast(this);
    body->addWidget(contentHost, 1);
    layout->addLayout(body, 1);
    connect(&m_devices, &DeviceService::changed, this, [this] {
        rebuildNav();
        updateFilter();
        if (m_route == "device" && !m_devices.find(m_currentId))
            navigate("all");
        else
            refreshDevice();
    });
    connect(&m_local, &LocalSystemService::changed, this, &MainWindow::updateFilter);
    if (!m_devices.devices().isEmpty())
        m_currentId = m_devices.devices().first().id;
    rebuildNav();
    if (m_currentId.isEmpty())
        navigate("all");
    else
        showDevice(m_currentId);
    loadSettings();
    connect(&m_sessions, &SessionService::failed, this, [this](const QString &error) {
        Ui::alert(m_session ? static_cast<QWidget *>(m_session) : this, QStringLiteral("会话操作失败"),
                  error);
    });
    auto *shortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(shortcut, &QShortcut::activated, this, [this] {
        navigate("all");
        m_search->setFocus();
    });
    auto *stop = new QShortcut(QKeySequence("Ctrl+Alt+Shift+F11"), this);
    connect(stop, &QShortcut::activated, this, [this] {
        if (m_session)
            m_session->close();
        notify(QStringLiteral("会话已停止"));
    });
#ifdef Q_OS_WIN
    Ui::enableChrome(this, true, true);
#endif
}

void MainWindow::navigate(const QString &route) {
    if (route.startsWith("device:")) {
        showDevice(route.mid(7));
        return;
    }
    m_route = route;
    auto *settings = findChild<QPushButton *>("settingsNav");
    settings->setChecked(route == "settings");
    if (route == "all" || route == "favorites") {
        m_favoritesOnly = route == "favorites";
        findChild<QLabel *>("listTitle")
            ->setText(m_favoritesOnly ? QStringLiteral("收藏设备") : QStringLiteral("全部设备"));
        m_search->clear();
        m_filter->setCurrentIndex(0);
        updateFilter();
        m_pages->setCurrentIndex(1);
    } else if (route == "start")
        m_pages->setCurrentIndex(2);
    else if (route == "settings")
        m_pages->setCurrentIndex(3);
    for (auto i = m_routes.cbegin(); i != m_routes.cend(); ++i)
        i.value()->setChecked(i.key() == route);
    updateNavIndicator();
}

void MainWindow::showDevice(const QString &id) {
    if (!m_devices.find(id))
        return;
    m_currentId = id;
    m_route = "device";
    m_pages->setCurrentIndex(0);
    findChild<QPushButton *>("settingsNav")->setChecked(false);
    refreshDevice();
    for (auto i = m_routes.cbegin(); i != m_routes.cend(); ++i)
        i.value()->setChecked(i.key() == "device:" + id);
    updateNavIndicator();
}

void MainWindow::refreshDevice() {
    auto *d = m_devices.find(m_currentId);
    if (!d)
        return;
    m_deviceTitle->setText(d->name);
    m_onlineBadge->setText(d->online ? QStringLiteral("● 在线") : QStringLiteral("● 离线"));
    m_onlineBadge->setProperty("offline", !d->online);
    m_onlineBadge->style()->unpolish(m_onlineBadge);
    m_onlineBadge->style()->polish(m_onlineBadge);
    m_favorite->setIcon(
        icon(d->favorite ? "star-filled" : "star", QColor(d->favorite ? "#3478ed" : "#999999")));
    m_preview->setDevice(d->name, d->online);
    m_connect->setEnabled(d->online && m_sessions.isAvailable());
}

void MainWindow::notify(const QString &s) {
    m_status->showMessage(s);
}

void MainWindow::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    if (m_status)
        m_status->reposition();
    QTimer::singleShot(0, this, [this] { updateNavIndicator(false); });
    if (m_settings && m_settings->value("rememberWindow", false).toBool() && !isMinimized())
        m_settings->setValue("windowSize", size());
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if (auto *dialog = Ui::activeDialog(); dialog && dialog->host() == this) {
        dialog->reject();
        e->ignore();
        return;
    }
    if (m_session && m_settings->value("confirmExit", true).toBool() &&
        !Ui::confirm(this, QStringLiteral("退出 EasyDesk"), QStringLiteral("退出将结束当前会话"),
                     QStringLiteral("退出"))) {
        e->ignore();
        return;
    }
    if (m_session)
        m_session->close();
    QWidget::closeEvent(e);
}

bool MainWindow::nativeEvent(const QByteArray &type, void *message, long *result) {
    if (Ui::chromeEvent(this, message, result, true))
        return true;
    return QWidget::nativeEvent(type, message, result);
}
