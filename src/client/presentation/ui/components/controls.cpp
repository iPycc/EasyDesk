#include "ui/ui.h"
#include "ui/components/icons.h"

namespace Ui {
QLabel *label(const QString &s, const char *role) {
    auto *w = new QLabel(s);
    w->setProperty("role", role);
    return w;
}
QPushButton *action(const QString &s, const QString &glyph, const char *kind) {
    auto *b = new QPushButton(s);
    b->setProperty("kind", kind);
    b->setMinimumHeight(36);
    b->setCursor(Qt::PointingHandCursor);
    if (!glyph.isEmpty())
        b->setIcon(icon(glyph, QString(kind) == "primary" ? QColor(Qt::white) : QColor()));
    b->setIconSize(QSize(18, 18));
    return b;
}
Toggle::Toggle(QWidget *parent) : QCheckBox(parent) {
    setFixedSize(44, 24);
    setCursor(Qt::PointingHandCursor);
    m_animation = new QPropertyAnimation(this, "position", this);
    m_animation->setDuration(130);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(this, &QCheckBox::toggled, this, [this](bool checked) {
        m_animation->stop();
        if (!isVisible()) {
            setPosition(checked ? 1 : 0);
            return;
        }
        m_animation->setStartValue(m_position);
        m_animation->setEndValue(checked ? 1 : 0);
        m_animation->start();
    });
}

void Toggle::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor track = isChecked() ? accent() : QColor(isDark() ? "#363636" : "#ffffff");
    QColor border = isChecked() ? accent() : QColor(isDark() ? "#a0a0a0" : "#888888");
    if (!isEnabled()) {
        track.setAlpha(110);
        border.setAlpha(110);
    }
    p.setPen(QPen(border, 1));
    p.setBrush(track);
    p.drawRoundedRect(QRectF(1, 2, 41, 20), 10, 10);
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? Qt::white : QColor(isDark() ? "#d0d0d0" : "#666666"));
    p.drawEllipse(QRectF(5 + 20 * m_position, 6, 12, 12));
}

NavButton::NavButton(const QString &title, const QString &glyph, QWidget *parent)
    : QPushButton(title, parent), m_glyph(glyph) {
    setProperty("kind", "nav");
    setIcon(::icon(glyph, accent()));
    setIconSize(QSize(20, 20));
    setCheckable(true);
    setMinimumHeight(36);
    setCursor(Qt::PointingHandCursor);
}

void NavButton::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    if (isChecked() || underMouse()) {
        p.setBrush(
            QColor(isDark() ? (isChecked() ? "#333333" : "#2d2d2d") : (isChecked() ? "#dfe8f2" : "#e5ecf3")));
        p.drawRoundedRect(rect(), 5, 5);
    }
    if (isChecked() && !property("sharedIndicator").toBool()) {
        p.setBrush(accent());
        p.drawRoundedRect(QRectF(0, (height() - 18) / 2, 3, 18), 1.5, 1.5);
    }
    bool group = property("kind") == "navGroup";
    QColor fg =
        group ? QColor(isDark() ? "#e2e2e2" : "#303944") : qApp->palette().color(QPalette::WindowText);
    int indent = property("depth").toInt() * 24;
    const bool platform = m_glyph == "windows" || m_glyph == "apple" || m_glyph == "linux";
    ::icon(m_glyph, platform || isChecked() ? accent() : fg)
        .paint(&p, QRect(QPoint(12 + indent, (height() - iconSize().height()) / 2), iconSize()));
    p.setPen(fg);
    p.setFont(font());
    int right = group ? 36 : 10;
    const int textLeft = 40 + indent + (indent > 0 ? 4 : 0);
    p.drawText(rect().adjusted(textLeft, 0, -right, 0), Qt::AlignVCenter | Qt::AlignLeft,
               fontMetrics().elidedText(text(), Qt::ElideRight, width() - textLeft - right));
    if (group)
        ::icon(property("expanded").toBool() ? "chevron-down" : "chevron", fg)
            .paint(&p, QRect(width() - 26, (height() - 14) / 2, 14, 14));
}

Toast::Toast(QWidget *parent) : QFrame(parent) {
    setObjectName("toast");
    setFixedWidth(300);
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(14, 12, 8, 12);
    l->setSpacing(10);
    auto *mark = new QLabel;
    mark->setPixmap(icon("check", accent()).pixmap(20, 20));
    l->addWidget(mark);
    m_text = label("");
    m_text->setWordWrap(true);
    l->addWidget(m_text, 1);
    auto *close = new QToolButton;
    close->setIcon(icon("close"));
    close->setAccessibleName(QStringLiteral("关闭通知"));
    close->setFixedSize(26, 26);
    l->addWidget(close);
    connect(close, &QToolButton::clicked, this, &QWidget::hide);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &QWidget::hide);
    hide();
}

void Toast::reposition() {
    if (parentWidget())
        move(parentWidget()->width() - width() - 24, 64);
}

void Toast::showMessage(const QString &message) {
    m_text->setText(message);
    adjustSize();
    reposition();
    show();
    raise();
    m_timer.start(3500);
}
} // namespace Ui
