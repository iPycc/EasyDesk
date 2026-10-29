#pragma once
#include "ui/windows/window_chrome.h"
namespace Ui {
class SessionWindow : public QWidget {
    Q_OBJECT
  public:
    SessionWindow(QWidget *placementAnchor, const QString &title);
    QVBoxLayout *body;
    void setFullscreen(bool enabled);
  signals:
    void closed();
    void fullscreenChanged(bool enabled);

  protected:
    bool nativeEvent(const QByteArray &, void *, long *) override;
    void showEvent(QShowEvent *) override;
    void changeEvent(QEvent *) override;
    void closeEvent(QCloseEvent *) override;

  private:
    QPointer<QWidget> m_anchor;
    QVBoxLayout *m_frame;
    TitleBar *m_titleBar;
    QRect m_windowedGeometry;
    bool m_centered = false, m_fullscreen = false;
    void updatePresentation(bool fullscreen);
};

} // namespace Ui
