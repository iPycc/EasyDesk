#include "ui/ui.h"
#include "ui/components/icons.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace Ui {
Menu::Menu(QWidget *parent) : QMenu(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setSeparatorsCollapsible(true);
    setMinimumWidth(200);
}

Menu::Menu(const QString &title, QWidget *parent) : Menu(parent) {
    setTitle(title);
}

void Menu::showEvent(QShowEvent *event) {
    QMenu::showEvent(event);
#ifdef Q_OS_WIN
    DWORD corner = 2;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 33, &corner, sizeof(corner));
    BOOL dark = isDark();
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &dark, sizeof(dark));
#endif
}

ComboBox::ComboBox(QWidget *parent) : QComboBox(parent) {
    setView(new QListView);
    setCursor(Qt::PointingHandCursor);
}

void ComboBox::showPopup() {
    if (!isEnabled() || count() == 0)
        return;
    if (!m_popup) {
        m_popup = new Menu(this);
        m_popup->setObjectName("comboFlyout");
        connect(m_popup, &QMenu::aboutToHide, this, [this] {
            setProperty("popupOpen", false);
            style()->unpolish(this);
            style()->polish(this);
            QComboBox::hidePopup();
        });
    }
    m_popup->clear();
    qDeleteAll(m_popup->findChildren<QActionGroup *>(QString(), Qt::FindDirectChildrenOnly));
    m_popup->setMinimumWidth(width());
    auto *group = new QActionGroup(m_popup);
    group->setExclusive(true);
    for (int index = 0; index < count(); ++index) {
        auto *choice = m_popup->addAction(itemIcon(index), itemText(index));
        choice->setCheckable(true);
        choice->setChecked(index == currentIndex());
        choice->setEnabled(model()
                               ->flags(model()->index(index, modelColumn(), rootModelIndex()))
                               .testFlag(Qt::ItemIsEnabled));
        group->addAction(choice);
        connect(choice, &QAction::triggered, this, [this, index] {
            setCurrentIndex(index);
            emit activated(index);
            emit textActivated(itemText(index));
        });
    }
    setProperty("popupOpen", true);
    style()->unpolish(this);
    style()->polish(this);
    m_popup->popup(mapToGlobal(QPoint(0, height() + 4)));
    if (currentIndex() >= 0)
        m_popup->setActiveAction(m_popup->actions().value(currentIndex()));
}

void ComboBox::hidePopup() {
    if (m_popup)
        m_popup->hide();
    QComboBox::hidePopup();
}
} // namespace Ui
