#pragma once
#include <QtWidgets>

namespace Ui {
void enableChrome(QWidget *window, bool resizable = true, bool minimize = true);
bool chromeEvent(QWidget *window, void *message, long *result, bool resizable = true);
void minimize(QWidget *window);
class TitleBar : public QFrame {
  public:
    TitleBar(QWidget *window, const QString &title, bool minimize = false);
    QHBoxLayout *items;

  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

} // namespace Ui
