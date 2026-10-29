#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

QWidget *MainWindow::buildStartPage() {
    QVBoxLayout *l;
    auto *p = page(&l);
    heading(l, QStringLiteral("开始远程"));
    l->addSpacing(5);
    auto *cols = row(20);
    auto *local = new QFrame;
    local->setObjectName("identityPanel");
    auto *a = column(local, 22, 14);
    a->addWidget(text(QStringLiteral("此设备"), "h3"));
    a->addWidget(text(m_sessions.statusText(), "caption"));
    a->addSpacing(12);
    a->addWidget(text(QStringLiteral("设备 ID"), "caption"));
    auto *idRow = row();
    auto *localId = text(spacedId(m_sessions.localIdentity().id), "largeId");
    idRow->addWidget(localId);
    connect(&m_sessions, &SessionService::identityChanged, localId,
            [this, localId] { localId->setText(spacedId(m_sessions.localIdentity().id)); });
    auto *cp = tool(QStringLiteral("复制设备 ID"), "copy");
    idRow->addWidget(cp);
    idRow->addStretch();
    a->addLayout(idRow);
    connect(cp, &QToolButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_sessions.localIdentity().id);
        notify(QStringLiteral("已复制设备 ID"));
    });
    a->addSpacing(8);
    a->addWidget(divider());
    a->addSpacing(8);
    a->addWidget(text(QStringLiteral("临时协助码"), "caption"));
    auto *codeRow = row();
    auto *code = text("•••• ••••", "largeId");
    code->setObjectName("localCode");
    codeRow->addWidget(code);
    codeRow->addStretch();
    auto *eye = tool(QStringLiteral("显示 / 隐藏协助码"), "eye");
    eye->setCheckable(true);
    codeRow->addWidget(eye);
    auto *refresh = tool(QStringLiteral("刷新协助码"), "refresh");
    codeRow->addWidget(refresh);
    a->addLayout(codeRow);
    auto updateCode = [this, code, eye] {
        QString value = m_sessions.localIdentity().assistanceCode;
        value.insert(4, ' ');
        code->setText(eye->isChecked() ? value : QStringLiteral("•••• ••••"));
    };
    connect(eye, &QToolButton::toggled, this, [updateCode](bool) { updateCode(); });
    connect(&m_sessions, &SessionService::identityChanged, code, updateCode);
    connect(refresh, &QToolButton::clicked, this, [this] {
        m_sessions.refreshAssistanceCode();
        notify(QStringLiteral("协助码已刷新"));
    });
    a->addStretch();
    cols->addWidget(local, 1);
    auto *connectPane = new QWidget;
    auto *b = column(connectPane, 0, 12);
    b->addWidget(text(QStringLiteral("连接远程设备"), "h3"));
    b->addSpacing(6);
    b->addWidget(text(QStringLiteral("对方设备 ID"), "formLabel"));
    m_remoteId = input(QStringLiteral("输入 9 位设备 ID"), "remoteId");
    m_remoteId->setMaxLength(11);
    b->addWidget(m_remoteId);
    b->addWidget(text(QStringLiteral("临时协助码"), "formLabel"));
    m_remoteCode = input(QStringLiteral("输入 8 位临时协助码"), "remoteCode");
    m_remoteCode->setEchoMode(QLineEdit::Password);
    m_remoteCode->setMaxLength(9);
    b->addWidget(m_remoteCode);
    auto *mode = new Ui::ComboBox;
    mode->setObjectName("connectionMode");
    mode->addItems({QStringLiteral("远程控制"), QStringLiteral("仅观看")});
    mode->setMinimumHeight(36);
    b->addWidget(mode);
    auto *error = text("", "error");
    error->setObjectName("connectError");
    error->setWordWrap(true);
    error->setMinimumHeight(18);
    b->addWidget(error);
    auto *go = button(QStringLiteral("开始连接"), "arrow", "primary");
    go->setObjectName("startConnect");
    go->setMinimumHeight(40);
    go->setEnabled(m_sessions.isAvailable());
    b->addWidget(go);
    connect(go, &QPushButton::clicked, this, &MainWindow::connectRemote);
    connect(m_remoteCode, &QLineEdit::returnPressed, this, &MainWindow::connectRemote);
    b->addStretch();
    cols->addWidget(connectPane, 1);
    l->addLayout(cols, 1);
    return p;
}
