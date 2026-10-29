#include "ui/windows/main_window.h"
#include "ui/components/layout_helpers.h"
#include "ui/components/icons.h"
using namespace Ui::Layout;
QWidget *MainWindow::buildListPage() {
    QVBoxLayout *l;
    auto *p = page(&l);
    p->setObjectName("listPage");
    auto *h = row();
    auto *title = text(QStringLiteral("全部设备"), "h1");
    title->setObjectName("listTitle");
    h->addWidget(title);
    h->addStretch();
    auto *add = button(QStringLiteral("添加设备"), "plus", "primary");
    add->setObjectName("addDeviceButton");
    h->addWidget(add);
    connect(add, &QPushButton::clicked, this, &MainWindow::addDevice);
    l->addLayout(h);
    auto *searchRow = row(12);
    m_search = input(QStringLiteral("搜索设备名称或 ID"), "deviceSearch");
    m_search->addAction(icon("search"), QLineEdit::LeadingPosition);
    searchRow->addWidget(m_search, 1);
    m_filter = new Ui::ComboBox;
    m_filter->addItems({QStringLiteral("全部状态"), QStringLiteral("仅在线")});
    m_filter->setMinimumWidth(115);
    searchRow->addWidget(m_filter);
    l->addLayout(searchRow);
    m_deviceList = new Ui::DeviceList;
    l->addWidget(m_deviceList, 1);
    connect(m_deviceList, &Ui::DeviceList::deviceActivated, this, &MainWindow::showDevice);
    connect(m_deviceList, &Ui::DeviceList::detailsRequested, this, &MainWindow::showDeviceDetails);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::updateFilter);
    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateFilter);
    return p;
}
void MainWindow::updateFilter() {
    if (m_deviceList)
        m_deviceList->setDevices(m_devices.devices(), m_local.information(), m_search->text().trimmed(),
                                 m_favoritesOnly, m_filter->currentIndex() == 1);
}
