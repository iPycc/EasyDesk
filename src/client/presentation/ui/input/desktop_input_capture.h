#pragma once

#include "features/sessions/session_service.h"

#include <QObject>
#include <QPointer>

#include <functional>

class QWidget;
class QKeyEvent;

class DesktopInputCapture final : public QObject {
    Q_OBJECT

  public:
    using TargetSizeProvider = std::function<QSize()>;
    DesktopInputCapture(QWidget &target, SessionService &session,
                        TargetSizeProvider targetSize = {}, QObject *parent = nullptr);
    void releaseInput();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    bool acceptsInput() const;
    bool mapPoint(const QPointF &point, QPoint &normalized) const;
    bool localShortcut(const QKeyEvent &event) const;

    QPointer<QWidget> m_target;
    QPointer<QWidget> m_window;
    SessionService &m_session;
    TargetSizeProvider m_targetSize;
    SessionInputStatus m_status;
};
