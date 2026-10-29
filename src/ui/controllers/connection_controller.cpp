#include "ui/windows/main_window.h"
#include <QtWidgets>

void MainWindow::connectRemote() {
    const QString id = QString(m_remoteId->text()).remove(' ');
    const QString code = QString(m_remoteCode->text()).remove(' ');
    auto *error = findChild<QLabel *>("connectError");
    const QString message = m_sessions.validateConnection(id, code);
    error->setText(message);
    if (!message.isEmpty())
        return;
    startSession(id);
}
void MainWindow::useExampleDevice() {
    for (const auto &device : m_devices.devices())
        if (device.online) {
            m_remoteId->setText(spacedId(device.id));
            m_remoteCode->setText(m_sessions.localIdentity().assistanceCode);
            notify(QStringLiteral("已填入示例设备"));
            return;
        }
}
