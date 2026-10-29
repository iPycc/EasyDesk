#include "data/mock/mock_transfer_service.h"

MockTransferService::MockTransferService(QObject *parent) : TransferService(parent) {
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &MockTransferService::advance);
}
void MockTransferService::start() {
    m_items = {{QStringLiteral("产品设计稿.fig"), "24.8 MB", 100, TransferState::Completed},
               {QStringLiteral("项目说明.pdf"), "3.2 MB", 42, TransferState::Running},
               {QStringLiteral("会议记录.docx"), "128 KB", 0, TransferState::Waiting}};
    m_paused = false;
    m_timer.start();
    emit changed();
}
void MockTransferService::pause() {
    m_timer.stop();
    m_paused = true;
    for (auto &item : m_items)
        if (item.state == TransferState::Running)
            item.state = TransferState::Paused;
    emit changed();
}
void MockTransferService::resume() {
    m_paused = false;
    for (auto &item : m_items)
        if (item.state == TransferState::Paused)
            item.state = TransferState::Running;
    m_timer.start();
    emit changed();
}
void MockTransferService::cancel() {
    m_timer.stop();
    m_paused = false;
    for (auto &item : m_items)
        if (item.state != TransferState::Completed)
            item.state = TransferState::Cancelled;
    emit changed();
}
void MockTransferService::advance() {
    for (auto &item : m_items) {
        if (item.state == TransferState::Waiting)
            item.state = TransferState::Running;
        if (item.state == TransferState::Running) {
            item.progress = qMin(100, item.progress + 4);
            if (item.progress == 100)
                item.state = TransferState::Completed;
            emit changed();
            return;
        }
    }
    m_timer.stop();
    emit changed();
}
