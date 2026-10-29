#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/desktop_view.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

void MainWindow::startSession(const QString &id) {
    auto *device = m_devices.find(id);
    if (device && !device->online) {
        notify(QStringLiteral("设备已离线"));
        return;
    }
    if (m_session) {
        m_session->raise();
        m_session->activateWindow();
        return;
    }
    const QString name = device ? device->name : spacedId(id);
    const bool simulated = m_sessions.telemetry().simulated;
    Ui::Dialog consent(this, QStringLiteral("连接设备"));
    consent.setObjectName("sessionConsent");
    consent.resize(440, 270);
    auto *c = consent.body;
    c->addWidget(text(name, "h2"));
    c->addWidget(text(spacedId(id), "muted"));
    auto *view = new QCheckBox(QStringLiteral("仅观看"));
    view->setChecked(findChild<QCheckBox *>("viewOnly")->isChecked());
    view->setObjectName("sessionViewOnly");
    if (m_route == "start" && findChild<QComboBox *>("connectionMode")->currentIndex() == 1)
        view->setChecked(true);
    c->addWidget(view);
    c->addWidget(text(simulated ? QStringLiteral("模拟桌面") : QString(), "caption"));
    auto *r = consent.actions;
    auto *cancel = button(QStringLiteral("取消"));
    auto *accept = button(QStringLiteral("连接"), "arrow", "primary");
    accept->setObjectName("dialogAccept");
    r->addWidget(accept, 1);
    r->addWidget(cancel, 1);
    connect(cancel, &QPushButton::clicked, &consent, &Ui::Dialog::reject);
    connect(accept, &QPushButton::clicked, &consent, &Ui::Dialog::accept);
    if (consent.exec() != Ui::Dialog::Accepted)
        return;
    auto *session = new Ui::SessionWindow(
        this, name + (simulated ? QStringLiteral(" · 模拟桌面") : QStringLiteral(" · 远程桌面")));
    session->setObjectName("sessionWindow");
    m_session = session;
    session->setAttribute(Qt::WA_DeleteOnClose);
    session->resize(960, 640);
    session->setMinimumSize(640, 420);
    auto *l = session->body;
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto *desktop = new DesktopView;
    desktop->setObjectName("sessionDesktop");
    desktop->setSession(true);
    desktop->setDevice(name, true);
    l->addWidget(desktop, 1);
    int initialFps = QList<int>{30, 60, 15}.value(findChild<QComboBox *>("fps")->currentIndex(), 30);
    QString initialQuality = findChild<QComboBox *>("quality")->currentText();
    SessionOptions requested;
    requested.frameRate = initialFps;
    requested.quality = initialQuality;
    requested.viewOnly = view->isChecked();
    m_sessions.start(id, requested);
    session->setProperty("fps", initialFps);
    session->setProperty("quality", initialQuality);
    session->setProperty("telemetryIsSimulated", m_sessions.telemetry().simulated);
    auto *title = dynamic_cast<Ui::TitleBar *>(session->findChild<QFrame *>("titleBar"));
    auto *signal = tool(QStringLiteral("连接信息"), "quality");
    signal->setObjectName("sessionSignal");
    signal->setIcon(icon("quality", QColor("#19a36a")));
    signal->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    signal->setFixedSize(82, 32);
    title->items->insertWidget(title->items->count() - 2, signal);
    auto *info = new Ui::Menu(signal);
    info->setObjectName("sessionTelemetry");
    auto *panel = new QWidget;
    panel->setObjectName("telemetryPanel");
    panel->setMinimumWidth(286);
    auto *stats = column(panel, 14, 14);
    auto *heading = row();
    heading->addWidget(text(QStringLiteral("连接信息"), "h3"));
    heading->addStretch();
    auto *simulation = text(QStringLiteral("模拟数据"), "caption");
    heading->addWidget(simulation);
    stats->addLayout(heading);
    auto metric = [stats](const QString &label, const QString &value, const QString &key) {
        auto *r = row(20);
        r->addWidget(text(label, "muted"));
        r->addStretch();
        auto *v = text(value, "telemetryValue");
        v->setObjectName(key);
        r->addWidget(v);
        stats->addLayout(r);
        return v;
    };
    auto *latency = metric(QStringLiteral("延迟"), {}, "sessionLatency");
    auto *localIp = metric(QStringLiteral("本机 IP"), {}, "sessionLocalIp");
    auto *peerIp = metric(QStringLiteral("对端 IP"), {}, "sessionPeerIp");
    auto *fpsValue = metric(QStringLiteral("帧率"), {}, "sessionFps");
    auto *loss = metric(QStringLiteral("丢包"), {}, "sessionPacketLoss");
    auto *transport = metric(QStringLiteral("传输方式"), {}, "sessionTransport");
    auto updateTelemetry = [session, signal, simulation, latency, localIp, peerIp, fpsValue, loss,
                            transport](const SessionTelemetry &value) {
        session->setProperty("telemetryIsSimulated", value.simulated);
        simulation->setVisible(value.simulated);
        signal->setText(QString::number(value.latencyMs) + " ms");
        latency->setText(QString::number(value.latencyMs) + " ms");
        localIp->setText(value.localIp);
        peerIp->setText(value.peerIp);
        fpsValue->setText(QString::number(value.frameRate) + " FPS");
        loss->setText(QString::number(value.packetLossPercent, 'f', 1) + "%");
        transport->setText(value.transport +
                           (value.peerToPeer ? QStringLiteral(" · P2P") : QStringLiteral(" · 中继")));
    };
    connect(&m_sessions, &SessionService::telemetryChanged, session, updateTelemetry);
    updateTelemetry(m_sessions.telemetry());
    auto *infoAction = new QWidgetAction(info);
    infoAction->setDefaultWidget(panel);
    info->addAction(infoAction);
    signal->setMenu(info);
    signal->setPopupMode(QToolButton::InstantPopup);
    auto *more = tool(QStringLiteral("会话选项"), "more");
    more->setObjectName("sessionMore");
    title->items->insertWidget(title->items->count() - 2, more);
    auto *menu = new Ui::Menu(more);
    menu->setObjectName("sessionMenu");
    auto *fpsMenu = new Ui::Menu(QStringLiteral("帧率"), menu);
    fpsMenu->setObjectName("sessionFpsMenu");
    fpsMenu->setIcon(icon("monitor"));
    menu->addMenu(fpsMenu);
    auto *fpsGroup = new QActionGroup(fpsMenu);
    fpsGroup->setExclusive(true);
    for (int fps : {15, 30, 60}) {
        auto *a = fpsMenu->addAction(QString::number(fps) + " FPS");
        a->setObjectName(QString("sessionFps%1").arg(fps));
        a->setCheckable(true);
        a->setChecked(fps == initialFps);
        fpsGroup->addAction(a);
        connect(a, &QAction::triggered, session, [this, fps] { m_sessions.setFrameRate(fps); });
    }
    auto *qualityMenu = new Ui::Menu(QStringLiteral("画质"), menu);
    qualityMenu->setIcon(icon("quality"));
    menu->addMenu(qualityMenu);
    auto *qualityGroup = new QActionGroup(qualityMenu);
    qualityGroup->setExclusive(true);
    for (const auto &quality :
         QStringList{QStringLiteral("清晰"), QStringLiteral("流畅"), QStringLiteral("节流")}) {
        auto *a = qualityMenu->addAction(quality);
        a->setObjectName("sessionQuality" + quality);
        a->setCheckable(true);
        a->setChecked(quality == initialQuality);
        qualityGroup->addAction(a);
        connect(a, &QAction::triggered, session, [this, quality] { m_sessions.setQuality(quality); });
    }
    connect(&m_sessions, &SessionService::optionsChanged, session,
            [session, fpsGroup, qualityGroup](const SessionOptions &options) {
                session->setProperty("fps", options.frameRate);
                session->setProperty("quality", options.quality);
                for (auto *a : fpsGroup->actions())
                    a->setChecked(a->text() == QString::number(options.frameRate) + " FPS");
                for (auto *a : qualityGroup->actions())
                    a->setChecked(a->text() == options.quality);
            });
    auto *full = menu->addAction(icon("fullscreen"), QStringLiteral("全屏"));
    full->setObjectName("sessionFullscreen");
    full->setCheckable(true);
    full->setShortcut(QKeySequence(Qt::Key_F11));
    full->setShortcutContext(Qt::WindowShortcut);
    session->addAction(full);
    connect(full, &QAction::triggered, session, &Ui::SessionWindow::setFullscreen);
    connect(session, &Ui::SessionWindow::fullscreenChanged, full, &QAction::setChecked);
    connect(menu, &QMenu::aboutToShow, session,
            [session, full] { full->setChecked(session->isFullScreen()); });
    menu->addSeparator();
    menu->addAction(icon("folder"), QStringLiteral("文件传输"), this, &MainWindow::openTransfers);
    auto *screens = new Ui::Menu(QStringLiteral("屏幕"), menu);
    screens->setIcon(icon("monitor"));
    menu->addMenu(screens);
    auto *screenGroup = new QActionGroup(screens);
    for (int n : {1, 2}) {
        auto *a = screens->addAction(QStringLiteral("屏幕 %1").arg(n));
        a->setCheckable(true);
        a->setChecked(n == 1);
        screenGroup->addAction(a);
        connect(a, &QAction::triggered, desktop, [desktop, n] { desktop->setScreen(n); });
    }
    auto *scale = new Ui::Menu(QStringLiteral("显示比例"), menu);
    menu->addMenu(scale);
    scale->addAction(QStringLiteral("适应窗口"), desktop, [desktop] { desktop->setOriginalSize(false); });
    scale->addAction(QStringLiteral("原始比例"), desktop, [desktop] { desktop->setOriginalSize(true); });
    menu->addSeparator();
    auto *exit = menu->addAction(icon("logout"), QStringLiteral("退出控制"), session, &QWidget::close);
    exit->setObjectName("sessionExit");
    more->setMenu(menu);
    more->setPopupMode(QToolButton::InstantPopup);
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), session);
    connect(esc, &QShortcut::activated, session, [session, full] {
        if (session->isFullScreen())
            session->setFullscreen(false);
        else
            session->close();
    });
    auto *localMenu = new QShortcut(QKeySequence("Ctrl+Alt+Shift+F12"), session);
    connect(localMenu, &QShortcut::activated, session, [session, more, menu] {
        menu->popup(
            session->isFullScreen()
                ? session->mapToGlobal(QPoint(qMax(0, session->width() - menu->sizeHint().width() - 12), 12))
                : more->mapToGlobal(QPoint(0, more->height() + 4)));
    });
    auto *stop = new QShortcut(QKeySequence("Ctrl+Alt+Shift+F11"), session);
    connect(stop, &QShortcut::activated, session, &QWidget::close);
    connect(session, &Ui::SessionWindow::closed, this, [this] {
        m_sessions.stop();
        m_session = nullptr;
        notify(QStringLiteral("会话已结束"));
    });
    session->show();
}
