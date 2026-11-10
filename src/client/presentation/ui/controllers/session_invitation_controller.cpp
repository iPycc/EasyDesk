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
    dialog.resize(560, request.requestedMode == SessionMode::Control ? 510 : 470);
    dialog.body->addWidget(text(spacedId(request.fromDeviceId), "h2"));
    dialog.body->addWidget(
        text(request.requestedMode == SessionMode::Control
                 ? QStringLiteral("请求查看并控制此电脑")
                 : QStringLiteral("请求查看此电脑"),
             "muted"));

    auto addFingerprint = [&dialog](const QString &title, const QString &value) {
        dialog.body->addWidget(text(title, "formLabel"));
        auto *label = text(value, "detailValue");
        label->setWordWrap(true);
        label->setMinimumHeight(label->fontMetrics().lineSpacing() * 3);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        dialog.body->addWidget(label);
    };
    addFingerprint(QStringLiteral("本机指纹"), request.localFingerprint);
    addFingerprint(QStringLiteral("对方指纹"), request.peerFingerprint);
    dialog.body->addWidget(text(request.identityVerified ? QStringLiteral("设备身份已核对")
                                                         : QStringLiteral("请通过其他可信方式与对方核对指纹。"),
                                "caption"));
    QCheckBox *remember = nullptr;
    if (request.canRememberIdentity) {
        remember = new QCheckBox(QStringLiteral("记住此设备身份"));
        remember->setObjectName("rememberIncomingIdentity");
        dialog.body->addWidget(remember);
    }

    QCheckBox *allowControl = nullptr;
    if (request.requestedMode == SessionMode::Control) {
        allowControl = new QCheckBox(QStringLiteral("键鼠控制暂不可用"));
        allowControl->setObjectName("incomingAllowControl");
        allowControl->setEnabled(false);
        allowControl->setToolTip(QStringLiteral("远程控制功能尚未就绪"));
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
            [this, &dialog, &decided, allowControl, remember] {
                const SessionMode mode = allowControl && allowControl->isChecked()
                                             ? SessionMode::Control
                                             : SessionMode::ViewOnly;
                if (m_sessions.acceptIncomingSession(mode, remember && remember->isChecked())) {
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
