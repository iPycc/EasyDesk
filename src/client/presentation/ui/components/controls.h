#pragma once
#include <QtWidgets>

namespace Ui {
QLabel *label(const QString &value, const char *role = "body");
QPushButton *action(const QString &value, const QString &glyph = {}, const char *kind = "secondary");
class Menu : public QMenu {
  public:
    explicit Menu(QWidget *parent = nullptr);
    Menu(const QString &title, QWidget *parent = nullptr);

  protected:
    void showEvent(QShowEvent *event) override;
};

class ComboBox : public QComboBox {
  public:
    explicit ComboBox(QWidget *parent = nullptr);
    void showPopup() override;
    void hidePopup() override;

  private:
    Menu *m_popup = nullptr;
};

class Toggle : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
  public:
    explicit Toggle(QWidget *parent = nullptr);
    qreal position() const {
        return m_position;
    }
    void setPosition(qreal value) {
        m_position = value;
        update();
    }
    QSize sizeHint() const override {
        return QSize(44, 24);
    }

  protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &pos) const override {
        return rect().contains(pos);
    }

  private:
    qreal m_position = 0;
    QPropertyAnimation *m_animation;
};

class ElidedLabel : public QLabel {
  public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    QSize minimumSizeHint() const override {
        return QSize(0, QLabel::minimumSizeHint().height());
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setFont(font());
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(contentsRect(), Qt::AlignVCenter | Qt::AlignLeft,
                   fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};
class NavButton : public QPushButton {
  public:
    NavButton(const QString &title, const QString &glyph, QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QString m_glyph;
};

class Toast : public QFrame {
  public:
    explicit Toast(QWidget *parent);
    void showMessage(const QString &message);
    void reposition();

  private:
    QLabel *m_text;
    QTimer m_timer;
};
} // namespace Ui
