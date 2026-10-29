#pragma once
#include <QWidget>
#include <QTimer>

// A native QWidget/HWND surface. The future renderer can attach to winId().
// The preview paints a synthetic desktop and never captures the user's screen.
class DesktopView : public QWidget {
    Q_OBJECT
  public:
    explicit DesktopView(QWidget *parent = nullptr);
    void setDevice(const QString &name, bool online);
    void setScreen(int screen);
    void setSession(bool session);
    void setOriginalSize(bool original);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QString m_name;
    bool m_online = true, m_session = false, m_original = false;
    int m_screen = 1, m_tick = 0;
    QTimer m_timer;
};
