#pragma once
#include <QtWidgets>
namespace Ui {
class Dialog : public QWidget {
    Q_OBJECT
  public:
    Dialog(QWidget *parent, const QString &title);
    ~Dialog() override;
    QVBoxLayout *body;
    QHBoxLayout *actions;
    enum { Rejected = 0, Accepted = 1 };
    bool hasBackdrop() const {
        return m_layer && m_layer->isVisible() && m_layer->geometry() == host()->rect();
    }
    int exec();
    void accept();
    void reject();
    QWidget *host() const {
        return parentWidget();
    }

  protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    void arrange();
    void finish(int);
    QPointer<QFrame> m_layer;
    QPointer<QWidget> m_previousFocus;
    QEventLoop *m_loop = nullptr;
    int m_result = Rejected;
    QList<QPointer<QShortcut>> m_shortcuts;
    QList<QPointer<QAction>> m_shortcutActions;
};
Dialog *activeDialog();
bool confirm(QWidget *parent, const QString &title, const QString &message,
             const QString &accept = QStringLiteral("确定"));
void alert(QWidget *parent, const QString &title, const QString &message);
QString prompt(QWidget *parent, const QString &title, const QString &value, bool *ok);
} // namespace Ui
