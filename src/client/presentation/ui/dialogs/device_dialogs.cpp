#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

void MainWindow::addDevice() {
    Ui::Dialog dialog(this, QStringLiteral("添加设备"));
    dialog.setObjectName("addDeviceDialog");
    dialog.resize(460, 446);
    auto *layout = dialog.body;
    layout->addWidget(text(QStringLiteral("设备 ID"), "formLabel"));
    auto *id = input(QStringLiteral("输入 9 位设备 ID"), "newDeviceId");
    id->setMaxLength(11);
    id->setClearButtonEnabled(true);
    id->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9 ]{0,11}"), id));
    layout->addWidget(id);
    layout->addWidget(text(QStringLiteral("设备名称"), "formLabel"));
    auto *name = input(QStringLiteral("为设备命名"), "newDeviceName");
    name->setMaxLength(40);
    name->setClearButtonEnabled(true);
    layout->addWidget(name);
    layout->addWidget(text(QStringLiteral("操作系统"), "formLabel"));
    auto *system = new Ui::ComboBox;
    system->setObjectName("newDeviceSystem");
    for (const auto &value : QStringList{"Windows 11", "macOS", "Linux"})
        system->addItem(icon(platformIcon(value), Ui::accent()), value, value);
    layout->addWidget(system);
    auto *error = text("", "error");
    error->setObjectName("addDeviceError");
    error->setWordWrap(true);
    layout->addWidget(error);
    auto *add = button(QStringLiteral("添加"), {}, "primary");
    add->setObjectName("dialogAccept");
    auto *cancel = button(QStringLiteral("取消"));
    cancel->setObjectName("dialogCancel");
    dialog.actions->addWidget(add, 1);
    dialog.actions->addWidget(cancel, 1);
    connect(cancel, &QPushButton::clicked, &dialog, &Ui::Dialog::reject);
    quint64 pending = 0;
    connect(add, &QPushButton::clicked, &dialog, [this, name, id, system, error, add, cancel, &pending] {
        QString value = id->text();
        value.remove(' ');
        if (value == m_sessions.localIdentity().id) {
            error->setText(QStringLiteral("此设备已在列表中"));
            return;
        }
        add->setEnabled(false);
        cancel->setEnabled(false);
        error->clear();
        pending = m_devices.add(value, name->text(), system->currentData().toString());
    });
    connect(&m_devices, &DeviceService::operationFinished, &dialog,
            [this, &dialog, &pending, error, add, cancel](quint64 request, bool ok, const QString &message) {
                if (request != pending)
                    return;
                add->setEnabled(true);
                cancel->setEnabled(true);
                if (!ok) {
                    error->setText(message);
                    return;
                }
                dialog.accept();
                notify(QStringLiteral("设备已添加"));
            });
    dialog.exec();
}
void MainWindow::showDeviceDetails(const QString &id) {
    const bool local = id == "local-machine";
    const auto *remote = m_devices.find(id);
    if (!local && !remote)
        return;
    if (local)
        m_local.refresh();
    const auto info = m_local.information();
    const QString deviceName = local ? info.displayName : remote->name;
    Ui::Dialog d(this, QStringLiteral("设备详情"));
    d.setObjectName("deviceDetailsDialog");
    d.resize(600, 490);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    auto *v = column(content, 0, 6);
    scroll->setWidget(content);
    d.body->addWidget(scroll, 1);
    auto field = [v](const QString &title, const QString &value) {
        auto *frame = new QFrame;
        frame->setObjectName("detailRow");
        auto *r = row(16);
        r->setContentsMargins(16, 12, 16, 12);
        frame->setLayout(r);
        r->addWidget(text(title), 0);
        auto *label = text(value.isEmpty() ? QStringLiteral("未读取到") : value, "detailValue");
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        r->addWidget(label, 1);
        v->addWidget(frame);
        return label;
    };
    auto *name = field(QStringLiteral("名称"), deviceName);
    name->setObjectName("detailsName");
    if (local) {
        field(QStringLiteral("计算机名称"), info.computerName)->setObjectName("detailsComputerName");
        field(QStringLiteral("系统"), info.operatingSystem);
        field(QStringLiteral("主板"), (info.boardManufacturer + "  " + info.boardProduct).trimmed())
            ->setObjectName("detailsBoard");
        field(QStringLiteral("处理器"), info.processor)->setObjectName("detailsProcessor");
        field(QStringLiteral("网卡"), info.networkAdapters.join("\n"))->setObjectName("detailsAdapters");
    } else {
        field(QStringLiteral("设备 ID"), spacedId(remote->id));
        field(QStringLiteral("系统"), remote->system);
        field(QStringLiteral("状态"), remote->online ? QStringLiteral("在线") : QStringLiteral("离线"));
        auto *note = text(QStringLiteral("此远程设备为预览数据"), "caption");
        v->addWidget(note);
    }
    v->addStretch();
    auto *rename = button(QStringLiteral("重命名"));
    rename->setObjectName("detailsRename");
    d.actions->addWidget(rename);
    d.actions->addStretch();
    auto *remove = button(QStringLiteral("删除设备"));
    remove->setObjectName("detailsRemove");
    remove->setEnabled(!local);
    d.actions->addWidget(remove);
    connect(rename, &QPushButton::clicked, &d, [this, local, id, name] {
        bool ok = false;
        const auto value = Ui::prompt(this, QStringLiteral("修改设备名称"), name->text(), &ok);
        if (!ok)
            return;
        if (local) {
            m_local.setDisplayName(value);
            name->setText(value);
        } else
            watchDeviceOperation(m_devices.rename(id, value), QStringLiteral("设备名称已更新"),
                                 [guard = QPointer<QLabel>(name), value] {
                                     if (guard)
                                         guard->setText(value);
                                 });
    });
    connect(remove, &QPushButton::clicked, &d, [this, &d, id, deviceName] {
        if (Ui::confirm(this, QStringLiteral("移除设备"), QStringLiteral("移除“%1”？").arg(deviceName),
                        QStringLiteral("移除"))) {
            d.accept();
            watchDeviceOperation(m_devices.remove(id), QStringLiteral("设备已移除"));
        }
    });
    d.exec();
}
void MainWindow::renameDevice() {
    const auto *device = m_devices.find(m_currentId);
    if (!device)
        return;
    const QString id = device->id, oldName = device->name;
    bool accepted = false;
    const QString name = Ui::prompt(this, QStringLiteral("重命名设备"), oldName, &accepted);
    if (accepted && !name.isEmpty())
        watchDeviceOperation(m_devices.rename(id, name), QStringLiteral("设备名称已更新"));
}
void MainWindow::removeDevice() {
    const auto *device = m_devices.find(m_currentId);
    if (!device)
        return;
    const QString id = device->id, name = device->name;
    if (!Ui::confirm(this, QStringLiteral("移除设备"), QStringLiteral("移除“%1”？").arg(name),
                     QStringLiteral("移除")))
        return;
    watchDeviceOperation(m_devices.remove(id), QStringLiteral("设备已移除"), [this] { navigate("all"); });
}
