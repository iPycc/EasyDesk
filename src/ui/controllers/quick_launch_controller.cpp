#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
using namespace Ui::Layout;
void MainWindow::refreshQuickLaunch() {
    auto *layout = qobject_cast<QVBoxLayout *>(m_quickHost->layout());
    while (auto *item = layout->takeAt(0)) {
        if (auto *childLayout = item->layout()) {
            while (auto *child = childLayout->takeAt(0)) {
                if (auto *w = child->widget()) {
                    w->hide();
                    w->setObjectName(QString());
                    w->deleteLater();
                }
                delete child;
            }
        }
        if (auto *w = item->widget()) {
            w->hide();
            w->setObjectName(QString());
            w->deleteLater();
        }
        delete item;
    }
    const auto entries = m_settings->value("quickLaunch/" + m_currentId).toStringList();
    if (entries.isEmpty()) {
        auto *add = new QPushButton;
        add->setObjectName("emptyQuickLaunch");
        add->setProperty("kind", "emptyLaunch");
        auto *emptyLayout = new QVBoxLayout(add);
        emptyLayout->setContentsMargins(8, 14, 8, 14);
        emptyLayout->setSpacing(6);
        emptyLayout->addStretch();
        auto *mark = new QLabel;
        mark->setPixmap(icon("plus").pixmap(26, 26));
        mark->setAlignment(Qt::AlignCenter);
        mark->setAttribute(Qt::WA_TransparentForMouseEvents);
        emptyLayout->addWidget(mark);
        auto *caption = text(QStringLiteral("添加"), "caption");
        caption->setAlignment(Qt::AlignCenter);
        caption->setAttribute(Qt::WA_TransparentForMouseEvents);
        emptyLayout->addWidget(caption);
        emptyLayout->addStretch();
        add->setAccessibleName(QStringLiteral("添加快捷启动"));
        add->setMinimumHeight(110);
        layout->addWidget(add);
        connect(add, &QPushButton::clicked, this, &MainWindow::addQuickLaunch);
    } else {
        auto *r = new QHBoxLayout;
        for (int i = 0; i + 1 < entries.size(); i += 2) {
            auto *b = button(entries[i], "play");
            b->setObjectName("quickLaunchItem");
            b->setToolTip(entries[i + 1]);
            r->addWidget(b);
            connect(b, &QPushButton::clicked, this,
                    [this] { notify(QStringLiteral("快捷启动已保存；远程执行将在接入后端后启用")); });
            auto *menu = new Ui::Menu(b);
            auto *remove = menu->addAction(QStringLiteral("移除"), this, [this, i] {
                auto values = m_settings->value("quickLaunch/" + m_currentId).toStringList();
                values.removeAt(i);
                values.removeAt(i);
                m_settings->setValue("quickLaunch/" + m_currentId, values);
                refreshQuickLaunch();
            });
            remove->setObjectName("removeQuickLaunch");
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(b, &QWidget::customContextMenuRequested, menu,
                    [b, menu](const QPoint &pos) { menu->exec(b->mapToGlobal(pos)); });
        }
        auto *add = button(QStringLiteral("添加"), "plus");
        r->addWidget(add);
        connect(add, &QPushButton::clicked, this, &MainWindow::addQuickLaunch);
        r->addStretch();
        layout->addLayout(r);
        layout->addStretch();
    }
}
void MainWindow::addQuickLaunch() {
    Ui::Dialog d(this, QStringLiteral("添加快捷启动"));
    d.setObjectName("quickLaunchDialog");
    d.resize(448, 340);
    auto *name = input(QStringLiteral("名称"), "launchName");
    name->setMaxLength(40);
    auto *path = input(QStringLiteral("远程应用路径"), "launchPath");
    d.body->addWidget(text(QStringLiteral("名称"), "formLabel"));
    d.body->addWidget(name);
    d.body->addWidget(text(QStringLiteral("应用路径"), "formLabel"));
    d.body->addWidget(path);
    auto *save = button(QStringLiteral("添加"), {}, "primary");
    save->setObjectName("dialogAccept");
    save->setEnabled(false);
    auto *cancel = button(QStringLiteral("取消"));
    d.actions->addWidget(save, 1);
    d.actions->addWidget(cancel, 1);
    auto changed = [name, path, save] {
        save->setEnabled(!name->text().trimmed().isEmpty() && !path->text().trimmed().isEmpty());
    };
    connect(name, &QLineEdit::textChanged, &d, changed);
    connect(path, &QLineEdit::textChanged, &d, changed);
    connect(save, &QPushButton::clicked, &d, &Ui::Dialog::accept);
    connect(cancel, &QPushButton::clicked, &d, &Ui::Dialog::reject);
    if (d.exec() == Ui::Dialog::Accepted) {
        auto values = m_settings->value("quickLaunch/" + m_currentId).toStringList();
        values << name->text().trimmed() << path->text().trimmed();
        m_settings->setValue("quickLaunch/" + m_currentId, values);
        refreshQuickLaunch();
    }
}
