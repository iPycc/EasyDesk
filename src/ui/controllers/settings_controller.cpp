#include "ui/windows/main_window.h"
#include <QtWidgets>

void MainWindow::loadSettings() {
    for (auto *c : m_tabs->findChildren<QCheckBox *>()) {
        if (c->objectName().isEmpty())
            continue;
        bool def = (c->objectName() == "confirmExit" || c->objectName() == "askEveryTime" ||
                    c->objectName() == "viewOnly" || c->objectName() == "allowFiles");
        c->setChecked(m_settings->value(c->objectName(), def).toBool());
    }
    for (auto *c : m_tabs->findChildren<QComboBox *>())
        if (!c->objectName().isEmpty())
            c->setCurrentIndex(qBound(0, m_settings->value(c->objectName(), 0).toInt(), c->count() - 1));
    findChild<QLineEdit *>("signalAddress")->setText(m_settings->value("signalAddress", "").toString());
    Ui::applyTheme(findChild<QComboBox *>("theme")->currentIndex() == 1);
    if (m_settings->value("rememberWindow", false).toBool())
        resize(m_settings->value("windowSize", QSize(920, 680)).toSize().expandedTo(minimumSize()));
}

void MainWindow::applySettings() {
    auto *address = findChild<QLineEdit *>("signalAddress");
    QString value = address->text().trimmed();
    QUrl url(value);
    if (!value.isEmpty() &&
        (!url.isValid() || url.scheme() != "wss" || url.host().isEmpty() || !url.userInfo().isEmpty())) {
        Ui::alert(this, QStringLiteral("检查服务地址"),
                  QStringLiteral("请输入 wss:// 开头的有效地址，或留空。"));
        address->setFocus();
        return;
    }
    for (auto *c : m_tabs->findChildren<QCheckBox *>())
        if (!c->objectName().isEmpty())
            m_settings->setValue(c->objectName(), c->isChecked());
    for (auto *c : m_tabs->findChildren<QComboBox *>())
        if (!c->objectName().isEmpty())
            m_settings->setValue(c->objectName(), c->currentIndex());
    m_settings->setValue("signalAddress", value);
    m_settings->sync();
    notify(m_settings->status() == QSettings::NoError ? QStringLiteral("设置已保存到本地")
                                                      : QStringLiteral("设置保存失败，请检查目录权限"));
}
