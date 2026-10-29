#pragma once
#include <QtWidgets>
#include "ui/ui.h"
#include "ui/components/icons.h"

namespace Ui {
namespace Layout {
inline QLabel *text(const QString &s, const char *role = "body") {
    auto *w = new QLabel(s);
    w->setProperty("role", role);
    w->setTextInteractionFlags(Qt::NoTextInteraction);
    return w;
}
inline QPushButton *button(const QString &s, const QString &ico = {}, const char *role = "secondary") {
    auto *b = new QPushButton(s);
    b->setProperty("kind", role);
    b->setCursor(Qt::PointingHandCursor);
    if (!ico.isEmpty())
        b->setIcon(icon(ico, QString(role) == "primary" ? QColor(Qt::white) : QColor(), 18));
    b->setIconSize(QSize(18, 18));
    b->setMinimumHeight(34);
    return b;
}
inline QToolButton *tool(const QString &tip, const QString &ico) {
    auto *b = new QToolButton;
    b->setIcon(icon(ico));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(30, 30);
    return b;
}
inline QFrame *divider() {
    auto *f = new QFrame;
    f->setObjectName("divider");
    f->setFixedHeight(1);
    return f;
}
inline QVBoxLayout *column(QWidget *w, int margin = 0, int spacing = 12) {
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(margin, margin, margin, margin);
    l->setSpacing(spacing);
    return l;
}
inline QHBoxLayout *row(int spacing = 8) {
    auto *l = new QHBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(spacing);
    return l;
}
inline QWidget *page(QVBoxLayout **layout) {
    auto *p = new QWidget;
    p->setObjectName("page");
    *layout = column(p, 24, 16);
    return p;
}
inline QLineEdit *input(const QString &hint, const QString &name) {
    auto *e = new QLineEdit;
    e->setPlaceholderText(hint);
    e->setObjectName(name);
    e->setAccessibleName(hint);
    e->setMinimumHeight(38);
    e->setClearButtonEnabled(true);
    return e;
}
inline void heading(QVBoxLayout *layout, const QString &title) {
    layout->addWidget(text(title, "h1"));
}

} // namespace Layout
} // namespace Ui
