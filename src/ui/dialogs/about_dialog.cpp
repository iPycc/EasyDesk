#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

void MainWindow::showAbout() {
    Ui::Dialog d(this, QStringLiteral("关于 EasyDesk"));
    d.setObjectName("aboutDialog");
    d.resize(450, 320);
    auto *h = row(16);
    auto *logo = new QLabel;
    logo->setPixmap(icon("logo").pixmap(56, 56));
    h->addWidget(logo);
    auto *copy = new QVBoxLayout;
    copy->addWidget(text("EasyDesk", "h2"));
    copy->addWidget(text(QStringLiteral("1.0.0 · 前端预览"), "muted"));
    h->addLayout(copy, 1);
    d.body->addLayout(h);
    auto *note = text(QStringLiteral("本机信息来自系统。远程设备、画面与传输为模拟数据。"), "muted");
    note->setWordWrap(true);
    d.body->addWidget(note);
    d.body->addWidget(text(QStringLiteral("开源许可见发布包 licenses 目录"), "muted"));
    auto *ok = button(QStringLiteral("知道了"), {}, "primary");
    ok->setObjectName("dialogAccept");
    d.actions->addStretch();
    d.actions->addWidget(ok);
    connect(ok, &QPushButton::clicked, &d, &Ui::Dialog::accept);
    d.exec();
}
