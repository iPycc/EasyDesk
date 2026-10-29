#include "ui/components/selection_indicator.h"
#include "ui/theme/theme.h"
namespace Ui {
SelectionIndicator::SelectionIndicator(QWidget *parent) : QWidget(parent), m_animation(this, "geometry") {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_animation.setDuration(200);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    hide();
}
void SelectionIndicator::slideTo(const QRect &target, bool animated) {
    if (target.isEmpty()) {
        m_animation.stop();
        hide();
        return;
    }
    if (!isVisible() || !animated) {
        m_animation.stop();
        setGeometry(target);
        show();
        raise();
        return;
    }
    if (m_animation.state() == QAbstractAnimation::Running && m_animation.endValue().toRect() == target)
        return;
    if (geometry() == target)
        return;
    m_animation.stop();
    m_animation.setStartValue(geometry());
    m_animation.setEndValue(target);
    m_animation.start();
    raise();
}
void SelectionIndicator::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(accent());
    p.drawRoundedRect(rect(), 1.5, 1.5);
}
AnimatedTabBar::AnimatedTabBar(QWidget *parent) : QTabBar(parent), m_indicator(new SelectionIndicator(this)) {
    setDrawBase(false);
    m_indicator->setObjectName("tabIndicator");
    connect(this, &QTabBar::currentChanged, this, [this] { updateIndicator(true); });
}
void AnimatedTabBar::updateIndicator(bool animate) {
    auto r = tabRect(currentIndex());
    m_indicator->slideTo(QRect(r.left() + 18, r.bottom() - 2, r.width() - 36, 3), animate);
}
void AnimatedTabBar::resizeEvent(QResizeEvent *e) {
    QTabBar::resizeEvent(e);
    updateIndicator(false);
}
void AnimatedTabBar::showEvent(QShowEvent *e) {
    QTabBar::showEvent(e);
    updateIndicator(false);
}
} // namespace Ui
