#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"

#include <QtWidgets>

using namespace Ui::Layout;

void MainWindow::showPeerIdentityPrompt(const PeerIdentityPrompt &prompt) {
    Ui::Dialog dialog(m_session ? static_cast<QWidget *>(m_session) : this, QStringLiteral("核对设备身份"));
    dialog.setObjectName("peerIdentityPrompt");
    dialog.resize(560, 450);
    dialog.body->addWidget(text(spacedId(prompt.deviceId), "h2"));
    auto addFingerprint = [&dialog](const QString &title, const QString &value) {
        dialog.body->addWidget(text(title, "formLabel"));
        auto *label = text(value, "detailValue");
        label->setWordWrap(true);
        label->setMinimumHeight(label->fontMetrics().lineSpacing() * 3);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        dialog.body->addWidget(label);
    };
    addFingerprint(QStringLiteral("本机指纹"), prompt.localFingerprint);
    addFingerprint(QStringLiteral("对方指纹"), prompt.peerFingerprint);
    dialog.body->addWidget(text(QStringLiteral("请通过其他可信方式与对方核对指纹。"), "caption"));
    QCheckBox *remember = nullptr;
    if (prompt.canRemember) {
        remember = new QCheckBox(QStringLiteral("记住此设备身份"));
        remember->setObjectName("rememberPeerIdentity");
        dialog.body->addWidget(remember);
    }
    auto *cancel = button(QStringLiteral("取消"));
    auto *confirm = button(QStringLiteral("已核对"), "check", "primary");
    confirm->setObjectName("confirmPeerIdentity");
    dialog.actions->addWidget(cancel, 1);
    dialog.actions->addWidget(confirm, 1);
    connect(cancel, &QPushButton::clicked, &dialog, &Ui::Dialog::reject);
    connect(confirm, &QPushButton::clicked, &dialog, &Ui::Dialog::accept);
    connect(&m_sessions, &SessionService::sessionCancelled, &dialog, &Ui::Dialog::reject);
    const bool confirmed = dialog.exec() == Ui::Dialog::Accepted;
    (void)m_sessions.confirmPeerIdentity(confirmed, confirmed && remember && remember->isChecked());
}
