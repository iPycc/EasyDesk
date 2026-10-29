#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
#include <QtWidgets>
using namespace Ui::Layout;

void MainWindow::openTransfers() {
    if (m_transferDialog) {
        m_transferDialog->raise();
        m_transferDialog->activateWindow();
        return;
    }
    if (m_transfers.items().isEmpty())
        m_transfers.start();
    auto *dialog = new Ui::Dialog(this, QStringLiteral("文件传输"));
    m_transferDialog = dialog;
    dialog->setObjectName("transfersDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(600, 400);
    auto *layout = dialog->body;
    auto *table = new QTableWidget;
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("文件"), QStringLiteral("大小"), QStringLiteral("进度")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setColumnWidth(1, 80);
    table->setColumnWidth(2, 160);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(48);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    layout->addWidget(table, 1);
    auto *actions = dialog->actions;
    auto *status = text("", "caption");
    actions->addWidget(status);
    actions->addStretch();
    auto *pause = button(QStringLiteral("暂停"));
    pause->setObjectName("pauseTransfers");
    auto *cancel = button(QStringLiteral("取消队列"));
    cancel->setObjectName("cancelTransfers");
    actions->addWidget(pause);
    actions->addWidget(cancel);

    auto update = [this, table, status, pause, cancel] {
        const auto &items = m_transfers.items();
        table->setRowCount(items.size());
        bool finished = true, cancelled = false;
        for (int index = 0; index < items.size(); ++index) {
            const auto &item = items[index];
            if (!table->item(index, 0))
                table->setItem(index, 0, new QTableWidgetItem(icon("file"), item.name));
            else
                table->item(index, 0)->setText(item.name);
            if (!table->item(index, 1))
                table->setItem(index, 1, new QTableWidgetItem(item.size));
            else
                table->item(index, 1)->setText(item.size);
            auto *bar = qobject_cast<QProgressBar *>(table->cellWidget(index, 2));
            if (!bar) {
                bar = new QProgressBar;
                bar->setObjectName("transferProgress");
                table->setCellWidget(index, 2, bar);
            }
            bar->setValue(item.progress);
            switch (item.state) {
            case TransferState::Completed:
                bar->setFormat(QStringLiteral("已完成"));
                break;
            case TransferState::Cancelled:
                bar->setFormat(QStringLiteral("已取消"));
                cancelled = true;
                break;
            case TransferState::Waiting:
                bar->setFormat(QStringLiteral("等待中"));
                finished = false;
                break;
            case TransferState::Paused:
                bar->setFormat(QStringLiteral("已暂停"));
                finished = false;
                break;
            case TransferState::Running:
                bar->setFormat("%p%");
                finished = false;
                break;
            }
        }
        const bool available = m_transfers.isAvailable();
        pause->setText(m_transfers.isPaused() ? QStringLiteral("继续") : QStringLiteral("暂停"));
        pause->setEnabled(available && !finished);
        cancel->setEnabled(available && !finished);
        status->setText(!available               ? QStringLiteral("文件通道未连接")
                        : items.isEmpty()         ? QStringLiteral("暂无传输任务")
                        : cancelled              ? QStringLiteral("队列已取消")
                        : finished               ? QStringLiteral("队列已完成")
                        : m_transfers.isPaused() ? QStringLiteral("已暂停")
                                                 : QStringLiteral("正在传输…"));
    };
    connect(&m_transfers, &TransferService::changed, dialog, update);
    update();
    connect(pause, &QPushButton::clicked, dialog, [this] {
        if (m_transfers.isPaused())
            m_transfers.resume();
        else
            m_transfers.pause();
    });
    connect(cancel, &QPushButton::clicked, &m_transfers, &TransferService::cancel);
    dialog->show();
}
