#include "ui/windows/main_window.h"

#include "ui/components/layout_helpers.h"

#include <QtWidgets>

using namespace Ui::Layout;

void MainWindow::showIncomingSessionRequest(const IncomingSessionRequest &request) {
    if (m_session) {
        m_sessions.rejectIncomingSession();
        return;
    }

    Ui::Dialog dialog(this, QStringLiteral("远程协助请求"));
    dialog.setObjectName("incomingSessionConsent");
    dialog.resize(440, request.requestedMode == SessionMode::Control ? 300 : 260);
    dialog.body->addWidget(text(spacedId(request.fromDeviceId), "h2"));
    dialog.body->addWidget(
        text(request.requestedMode == SessionMode::Control
                 ? QStringLiteral("请求查看并控制此电脑")
                 : QStringLiteral("请求查看此电脑"),
             "muted"));

    QCheckBox *allowControl = nullptr;
    if (request.requestedMode == SessionMode::Control) {
        allowControl = new QCheckBox(QStringLiteral("允许键盘和鼠标控制"));
        allowControl->setObjectName("incomingAllowControl");
        dialog.body->addWidget(allowControl);
    }

    auto *reject = button(QStringLiteral("拒绝"));
    auto *accept = button(QStringLiteral("允许一次"), "check", "primary");
    reject->setObjectName("incomingReject");
    accept->setObjectName("incomingAccept");
    dialog.actions->addWidget(reject, 1);
    dialog.actions->addWidget(accept, 1);

    bool decided = false;
    connect(reject, &QPushButton::clicked, &dialog, [this, &dialog, &decided] {
        if (m_sessions.rejectIncomingSession()) {
            decided = true;
            dialog.reject();
        }
    });
    connect(accept, &QPushButton::clicked, &dialog,
            [this, &dialog, &decided, allowControl] {
                const SessionMode mode = allowControl && allowControl->isChecked()
                                             ? SessionMode::Control
                                             : SessionMode::ViewOnly;
                if (m_sessions.acceptIncomingSession(mode)) {
                    decided = true;
                    dialog.accept();
                }
            });
    connect(&m_sessions, &SessionService::incomingSessionCancelled,
            &dialog, [&dialog, &decided] {
                decided = true;
                dialog.reject();
            });

    dialog.exec();
    if (!decided)
        m_sessions.rejectIncomingSession();
}
