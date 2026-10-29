#include "ui/ui.h"
#include "ui/components/icons.h"
namespace Ui {
namespace {
QList<QPointer<Dialog>> dialogs;
}
Dialog *activeDialog() {
    for (int i = dialogs.size() - 1; i >= 0; --i)
        if (dialogs[i] && dialogs[i]->isVisible())
            return dialogs[i];
    return nullptr;
}
Dialog::Dialog(QWidget *parent, const QString &title) : QWidget(parent->window(), Qt::Widget) {
    setObjectName("contentDialog");
    setProperty("contentDialog", true);
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle(title);
    setFocusPolicy(Qt::StrongFocus);
    m_layer = new QFrame(parentWidget());
    m_layer->setObjectName("modalLayer");
    m_layer->hide();
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);
    auto *content = new QWidget;
    content->setObjectName("dialogBody");
    body = new QVBoxLayout(content);
    body->setContentsMargins(24, 24, 24, 24);
    body->setSpacing(14);
    auto *header = new QHBoxLayout;
    header->addWidget(label(title, "h2"), 1);
    auto *close = new QToolButton;
    close->setIcon(icon("close"));
    close->setAccessibleName(QStringLiteral("关闭"));
    close->setFixedSize(28, 28);
    header->addWidget(close);
    connect(close, &QToolButton::clicked, this, &Dialog::reject);
    body->addLayout(header);
    root->addWidget(content, 1);
    auto *footer = new QFrame;
    footer->setObjectName("dialogFooter");
    actions = new QHBoxLayout(footer);
    actions->setContentsMargins(24, 18, 24, 18);
    actions->setSpacing(8);
    root->addWidget(footer);
    parentWidget()->installEventFilter(this);
    hide();
}
Dialog::~Dialog() {
    hide();
    if (m_loop)
        m_loop->quit();
    delete m_layer;
}
void Dialog::arrange() {
    if (!parentWidget())
        return;
    m_layer->setGeometry(parentWidget()->rect());
    QSize wanted = size().expandedTo(minimumSizeHint()), available = parentWidget()->size() - QSize(48, 48);
    resize(qMin(wanted.width(), available.width()), qMin(wanted.height(), available.height()));
    move((parentWidget()->width() - width()) / 2, (parentWidget()->height() - height()) / 2);
}
void Dialog::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_previousFocus = QApplication::focusWidget();
    dialogs.append(this);
    for (auto *s : parentWidget()->findChildren<QShortcut *>())
        if (s->isEnabled() && !isAncestorOf(s->parentWidget())) {
            m_shortcuts << s;
            s->setEnabled(false);
        }
    for (auto *a : parentWidget()->findChildren<QAction *>())
        if (a->isEnabled() && !a->shortcut().isEmpty()) {
            m_shortcutActions << a;
            a->setEnabled(false);
        }
    qApp->installEventFilter(this);
    arrange();
    m_layer->show();
    m_layer->raise();
    raise();
    setFocus();
    auto edits = findChildren<QLineEdit *>();
    if (!edits.isEmpty()) {
        edits.first()->setFocus();
        edits.first()->selectAll();
    }
}
void Dialog::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (m_layer)
        m_layer->hide();
    qApp->removeEventFilter(this);
    for (auto s : m_shortcuts)
        if (s)
            s->setEnabled(true);
    for (auto a : m_shortcutActions)
        if (a)
            a->setEnabled(true);
    m_shortcuts.clear();
    m_shortcutActions.clear();
    dialogs.removeAll(this);
    if (m_previousFocus)
        m_previousFocus->setFocus();
}
bool Dialog::eventFilter(QObject *object, QEvent *event) {
    if (object == parentWidget() && event->type() == QEvent::Resize)
        arrange();
    if (isVisible() && activeDialog() == this && event->type() == QEvent::FocusIn) {
        auto *w = qobject_cast<QWidget *>(object);
        if (w && w->window() == host() && w != this && !isAncestorOf(w))
            setFocus();
    }
    auto *target = qobject_cast<QWidget *>(object);
    if (isVisible() && activeDialog() == this && event->type() == QEvent::KeyPress && target &&
        target->window() == host() && !QApplication::activePopupWidget()) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
            QList<QWidget *> stops;
            for (auto *w : findChildren<QWidget *>())
                if (w->isVisible() && w->isEnabled() && (w->focusPolicy() & Qt::TabFocus))
                    stops << w;
            if (!stops.isEmpty()) {
                int index = stops.indexOf(focusWidget());
                int step =
                    (key->key() == Qt::Key_Backtab || key->modifiers().testFlag(Qt::ShiftModifier)) ? -1 : 1;
                stops[(index + step + stops.size()) % stops.size()]->setFocus();
            }
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}
int Dialog::exec() {
    QEventLoop loop;
    m_loop = &loop;
    m_result = Rejected;
    show();
    loop.exec();
    m_loop = nullptr;
    return m_result;
}
void Dialog::finish(int result) {
    m_result = result;
    hide();
    if (m_loop)
        m_loop->quit();
    else if (testAttribute(Qt::WA_DeleteOnClose))
        deleteLater();
}
void Dialog::accept() {
    finish(Accepted);
}
void Dialog::reject() {
    finish(Rejected);
}
void Dialog::closeEvent(QCloseEvent *event) {
    reject();
    event->accept();
}
bool confirm(QWidget *parent, const QString &title, const QString &message, const QString &accept) {
    Dialog d(parent, title);
    d.setObjectName("confirmDialog");
    d.resize(440, 240);
    auto *copy = label(message);
    copy->setWordWrap(true);
    d.body->addWidget(copy, 1);
    auto *ok = action(accept, {}, "primary");
    ok->setObjectName("dialogAccept");
    auto *cancel = action(QStringLiteral("取消"));
    cancel->setObjectName("dialogCancel");
    d.actions->addWidget(ok, 1);
    d.actions->addWidget(cancel, 1);
    QObject::connect(cancel, &QPushButton::clicked, &d, &Dialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &d, &Dialog::accept);
    return d.exec() == Dialog::Accepted;
}
void alert(QWidget *parent, const QString &title, const QString &message) {
    Dialog d(parent, title);
    d.resize(440, 230);
    auto *copy = label(message);
    copy->setWordWrap(true);
    d.body->addWidget(copy, 1);
    auto *ok = action(QStringLiteral("知道了"), {}, "primary");
    d.actions->addStretch();
    d.actions->addWidget(ok);
    QObject::connect(ok, &QPushButton::clicked, &d, &Dialog::accept);
    d.exec();
}
QString prompt(QWidget *parent, const QString &title, const QString &value, bool *ok) {
    Dialog d(parent, title);
    d.resize(448, 250);
    d.setObjectName("renameDialog");
    auto *edit = new QLineEdit(value);
    edit->setObjectName("renameInput");
    edit->setMaxLength(40);
    edit->setClearButtonEnabled(true);
    edit->setMinimumHeight(36);
    d.body->addWidget(edit);
    auto *hint = new QHBoxLayout;
    hint->addWidget(label(QStringLiteral("最多可输入 40 个字符"), "caption"));
    hint->addStretch();
    auto *count = label("", "caption");
    hint->addWidget(count);
    d.body->addLayout(hint);
    auto *save = action(QStringLiteral("保存更改"), {}, "primary");
    save->setObjectName("dialogAccept");
    auto *cancel = action(QStringLiteral("取消"));
    cancel->setObjectName("dialogCancel");
    auto changed = [edit, count, save] {
        count->setText(QString::number(edit->text().size()) + "/40");
        save->setEnabled(!edit->text().trimmed().isEmpty());
    };
    QObject::connect(edit, &QLineEdit::textChanged, &d, changed);
    changed();
    d.actions->addWidget(save, 1);
    d.actions->addWidget(cancel, 1);
    QObject::connect(cancel, &QPushButton::clicked, &d, &Dialog::reject);
    QObject::connect(save, &QPushButton::clicked, &d, &Dialog::accept);
    QObject::connect(edit, &QLineEdit::returnPressed, save, &QPushButton::click);
    *ok = d.exec() == Dialog::Accepted;
    return edit->text().trimmed();
}
} // namespace Ui
