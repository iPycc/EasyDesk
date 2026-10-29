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
    copy->addWidget(text(QCoreApplication::applicationVersion(), "muted"));
    h->addLayout(copy, 1);
    d.body->addLayout(h);
    auto *note = text(QStringLiteral("信令未连接，远程会话和文件通道尚不可用。"), "muted");
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
