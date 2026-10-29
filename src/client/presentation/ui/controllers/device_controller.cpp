#include "ui/windows/main_window.h"

void MainWindow::watchDeviceOperation(quint64 request, const QString &success,
                                      std::function<void()> onSuccess) {
    auto *scope = new QObject(this);
    connect(&m_devices, &DeviceService::operationFinished, scope,
            [this, scope, request, success, onSuccess](quint64 completed, bool ok, const QString &error) {
                if (completed != request)
                    return;
                scope->deleteLater();
                if (!ok) {
                    Ui::alert(this, QStringLiteral("设备操作失败"), error);
                    return;
                }
                if (onSuccess)
                    onSuccess();
                if (!success.isEmpty())
                    notify(success);
            });
}
void MainWindow::toggleFavorite(const QString &id) {
    watchDeviceOperation(m_devices.toggleFavorite(id), {});
}
