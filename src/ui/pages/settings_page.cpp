#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

QWidget *MainWindow::buildSettingsPage() {
    QVBoxLayout *l;
    auto *p = page(&l);
    p->setObjectName("settingsPage");
    l->setSpacing(18);
    auto *header = row(12);
    auto *gear = new QLabel;
    gear->setPixmap(icon("settings", Ui::accent()).pixmap(30, 30));
    header->addWidget(gear);
    header->addWidget(text(QStringLiteral("设置"), "h1"), 1);
    l->addLayout(header);
    m_tabs = new Ui::SettingsTabs;
    m_tabs->setObjectName("settingsTabs");
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setDrawBase(false);
    l->addWidget(m_tabs, 1);
    auto tab = [this](const QString &name) {
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *w = new QWidget;
        auto *v = column(w, 0, 5);
        v->setContentsMargins(0, 16, 6, 8);
        scroll->setWidget(w);
        m_tabs->addTab(scroll, name);
        return v;
    };
    auto group = [](QVBoxLayout *v, const QString &name) {
        auto *h = text(name, "sectionTitle");
        h->setContentsMargins(0, 10, 0, 6);
        v->addWidget(h);
    };
    auto setting = [](QVBoxLayout *v, const QString &glyph, const QString &title, QWidget *control) {
        auto *w = new QFrame;
        w->setObjectName("settingRow");
        w->setMinimumHeight(66);
        auto *r = new QHBoxLayout(w);
        r->setContentsMargins(18, 12, 18, 12);
        r->setSpacing(16);
        auto *mark = new QLabel;
        mark->setProperty("glyph", glyph);
        mark->setFixedSize(24, 24);
        mark->setPixmap(icon(glyph).pixmap(22, 22));
        r->addWidget(mark);
        r->addWidget(text(title, "settingTitle"), 1);
        r->addWidget(control);
        v->addWidget(w);
    };
    auto check = [setting](QVBoxLayout *v, const QString &name, const QString &title, const QString &glyph,
                           bool initial) {
        auto *c = new Ui::Toggle;
        c->setObjectName(name);
        c->setAccessibleName(title);
        c->setChecked(initial);
        setting(v, glyph, title, c);
        return c;
    };
    auto combo = [setting](QVBoxLayout *v, const QString &name, const QString &title, const QString &glyph,
                           const QStringList &values) {
        auto *c = new Ui::ComboBox;
        c->setView(new QListView);
        c->setObjectName(name);
        c->setAccessibleName(title);
        c->addItems(values);
        c->setFixedWidth(174);
        c->setMinimumHeight(34);
        setting(v, glyph, title, c);
        return c;
    };
    auto *g = tab(QStringLiteral("常规"));
    group(g, QStringLiteral("外观"));
    combo(g, "language", QStringLiteral("语言"), "language", {QStringLiteral("简体中文")});
    auto *theme = combo(g, "theme", QStringLiteral("应用主题"), "sun",
                        {QStringLiteral("浅色"), QStringLiteral("深色")});
    group(g, QStringLiteral("窗口行为"));
    check(g, "rememberWindow", QStringLiteral("记住窗口大小"), "window", false);
    check(g, "confirmExit", QStringLiteral("会话中退出前确认"), "logout", true);
    g->addStretch();
    connect(theme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_settings->setValue("theme", index);
        Ui::applyTheme(index == 1);
    });
    auto *s = tab(QStringLiteral("安全"));
    group(s, QStringLiteral("连接权限"));
    check(s, "askEveryTime", QStringLiteral("连接前确认"), "shield", true)->setEnabled(false);
    check(s, "viewOnly", QStringLiteral("默认仅观看"), "eye", true);
    check(s, "allowFiles", QStringLiteral("接收文件前确认"), "folder", true);
    combo(s, "codeExpiry", QStringLiteral("临时码有效期"), "clock",
          {QStringLiteral("10 分钟"), QStringLiteral("5 分钟"), QStringLiteral("30 分钟")});
    s->addStretch();
    auto *k = tab(QStringLiteral("键盘"));
    group(k, QStringLiteral("输入"));
    combo(k, "keyboardMode", QStringLiteral("输入模式"), "keyboard",
          {QStringLiteral("物理按键"), QStringLiteral("文本输入（IME）")});
    check(k, "forwardShortcuts", QStringLiteral("发送系统组合键"), "keyboard", false);
    group(k, QStringLiteral("快捷键"));
    for (const auto &pair :
         QList<QPair<QString, QString>>{{QStringLiteral("打开会话菜单"), "Ctrl + Alt + Shift + F12"},
                                        {QStringLiteral("停止会话"), "Ctrl + Alt + Shift + F11"},
                                        {QStringLiteral("搜索设备"), "Ctrl + F"},
                                        {QStringLiteral("退出全屏"), "F11 / Esc"}})
        setting(k, "keyboard", pair.first, text(pair.second, "shortcut"));
    k->addStretch();
    auto *n = tab(QStringLiteral("网络"));
    group(n, QStringLiteral("画面"));
    combo(n, "quality", QStringLiteral("默认画质"), "quality",
          {QStringLiteral("清晰"), QStringLiteral("流畅"), QStringLiteral("节流")});
    combo(n, "fps", QStringLiteral("帧率上限"), "monitor",
          {QStringLiteral("30 FPS"), QStringLiteral("60 FPS"), QStringLiteral("15 FPS")});
    group(n, QStringLiteral("连接"));
    combo(n, "path", QStringLiteral("连接偏好"), "network",
          {QStringLiteral("自动选择"), QStringLiteral("优先直连"), QStringLiteral("优先中继")});
    auto *address = input(QStringLiteral("wss://signal.example.com"), "signalAddress");
    address->setFixedWidth(235);
    setting(n, "network", QStringLiteral("信令地址"), address);
    n->addStretch();
    auto *footer = row();
    auto *reset = button(QStringLiteral("恢复默认"), "refresh");
    reset->setObjectName("resetSettings");
    footer->addWidget(reset);
    footer->addStretch();
    auto *save = button(QStringLiteral("保存设置"), "check", "primary");
    save->setObjectName("saveSettings");
    footer->addWidget(save);
    l->addLayout(footer);
    connect(save, &QPushButton::clicked, this, &MainWindow::applySettings);
    connect(reset, &QPushButton::clicked, this, [this] {
        for (const auto &key :
             QStringList{"theme", "language", "rememberWindow", "confirmExit", "askEveryTime", "viewOnly",
                         "allowFiles", "codeExpiry", "keyboardMode", "forwardShortcuts", "quality", "fps",
                         "path", "signalAddress"})
            m_settings->remove(key);
        loadSettings();
        notify(QStringLiteral("已恢复默认设置"));
    });
    return p;
}
