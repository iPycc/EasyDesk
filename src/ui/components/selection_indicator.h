#pragma once
#include <QtWidgets>
namespace Ui {
class SelectionIndicator : public QWidget {
  public:
    explicit SelectionIndicator(QWidget *parent);
    void slideTo(const QRect &target, bool animated = true);
    bool animating() const {
        return m_animation.state() == QAbstractAnimation::Running;
    }

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QPropertyAnimation m_animation;
};
class AnimatedTabBar : public QTabBar {
  public:
    explicit AnimatedTabBar(QWidget *parent = nullptr);
    SelectionIndicator *indicator() const {
        return m_indicator;
    }

  protected:
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;

  private:
    void updateIndicator(bool animate);
    SelectionIndicator *m_indicator;
};
class SettingsTabs : public QTabWidget {
  public:
    explicit SettingsTabs(QWidget *parent = nullptr) : QTabWidget(parent) {
        setTabBar(new AnimatedTabBar);
    }
};
} // namespace Ui
