#pragma once
#include <QWidget>

// Native QWidget/HWND render target. The media renderer attaches to winId().
class DesktopView : public QWidget {
    Q_OBJECT
  public:
    explicit DesktopView(QWidget *parent = nullptr);
    void setDevice(const QString &name, bool online);
    void setScreen(int screen);
    void setSession(bool session);
    void setOriginalSize(bool original);
    void setExternalRendering(bool enabled);

    QPaintEngine *paintEngine() const override;

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    bool m_online = false, m_session = false, m_original = false;
    bool m_externalRendering = false;
    int m_screen = 1;
};
