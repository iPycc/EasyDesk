#include "ui/components/device_list.h"
#include "ui/components/icons.h"
#include "ui/components/controls.h"
namespace Ui {
namespace {
class DeviceNameButton : public QPushButton {
  public:
    explicit DeviceNameButton(const QString &text) : QPushButton(text) {
        setToolTip(text);
    }
    QSize minimumSizeHint() const override {
        return QSize(0, 38);
    }
    QSize sizeHint() const override {
        ensurePolished();
        return QSize(qMin(fontMetrics().horizontalAdvance(text()) + 12, 320), 38);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text = fontMetrics().elidedText(text(), Qt::ElideRight, qMax(0, width() - 4));
        QPainter p(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &p, this);
    }
};
} // namespace

DeviceList::DeviceList(QWidget *parent) : QScrollArea(parent) {
    setObjectName("deviceList");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
void DeviceList::setDevices(const QVector<Device> &devices, const LocalComputerInfo &local,
                            const QString &search, bool favorites, bool onlineOnly) {
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 6, 0);
    layout->setSpacing(6);
    m_count = 0;
    struct Entry {
        QString id, name, system;
        bool local;
    };
    QList<Entry> computers, mobiles;
    auto matches = [&](const QString &name, const QString &id) {
        return search.isEmpty() || name.contains(search, Qt::CaseInsensitive) || id.contains(search);
    };
    if (!favorites && matches(local.displayName, local.computerName))
        computers.append({"local-machine", local.displayName, local.operatingSystem, true});
    for (const auto &d : devices) {
        if ((favorites && !d.favorite) || (onlineOnly && !d.online) || !matches(d.name, d.id))
            continue;
        auto &group = d.category == DeviceCategory::Mobile ? mobiles : computers;
        group.append({d.id, d.name, d.system, false});
    }
    auto group = [&](const QString &title, const QString &key, const QList<Entry> &items) {
        if (items.isEmpty())
            return;
        m_count += items.size();
        auto *header = new QToolButton;
        header->setObjectName("deviceGroup-" + key);
        header->setText(title + " " + QString::number(items.size()));
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setCheckable(true);
        header->setChecked(m_expanded.value(key, true));
        header->setIcon(icon(header->isChecked() ? "chevron-down" : "chevron"));
        header->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        layout->addWidget(header);
        auto *rows = new QWidget;
        auto *v = new QVBoxLayout(rows);
        v->setContentsMargins(0, 0, 0, 14);
        v->setSpacing(5);
        rows->setVisible(header->isChecked());
        connect(header, &QToolButton::toggled, this, [this, rows, header, key](bool expanded) {
            m_expanded[key] = expanded;
            rows->setVisible(expanded);
            header->setIcon(icon(expanded ? "chevron-down" : "chevron"));
        });
        for (const auto &item : items) {
            auto *card = new QFrame;
            card->setObjectName("deviceRow");
            card->setProperty("deviceId", item.id);
            card->setMinimumHeight(58);
            auto *r = new QHBoxLayout(card);
            r->setContentsMargins(14, 8, 12, 8);
            r->setSpacing(12);
            auto *badge = new QLabel;
            badge->setObjectName("platformBadge");
            badge->setFixedSize(32, 32);
            badge->setAlignment(Qt::AlignCenter);
            badge->setPixmap(icon(platformIcon(item.system), Qt::white).pixmap(22, 22));
            r->addWidget(badge);
            auto *open = new DeviceNameButton(item.name);
            if (item.local)
                open->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
            open->setObjectName("deviceRowOpen-" + item.id);
            open->setProperty("kind", "rowTitle");
            open->setAccessibleName(item.name);
            if (!item.local)
                open->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            r->addWidget(open, item.local ? 0 : 1);
            if (item.local) {
                auto *tag = label(QStringLiteral("本机"), "localTag");
                tag->setFixedHeight(20);
                r->addWidget(tag, 0, Qt::AlignVCenter);
                r->addStretch();
            }
            auto *info = new QToolButton;
            info->setObjectName("deviceInfo-" + item.id);
            info->setIcon(icon("info"));
            info->setAccessibleName(QStringLiteral("设备详情：") + item.name);
            info->setToolTip(QStringLiteral("设备详情"));
            info->setFixedSize(32, 32);
            r->addWidget(info);
            connect(info, &QToolButton::clicked, this, [this, item] { emit detailsRequested(item.id); });
            connect(open, &QPushButton::clicked, this, [this, item] {
                if (item.local)
                    emit detailsRequested(item.id);
                else
                    emit deviceActivated(item.id);
            });
            if (!item.local) {
                auto *go = new QToolButton;
                go->setIcon(icon("chevron"));
                go->setAccessibleName(QStringLiteral("打开设备：") + item.name);
                go->setFixedSize(26, 32);
                r->addWidget(go);
                connect(go, &QToolButton::clicked, this, [this, item] { emit deviceActivated(item.id); });
            } else {
                auto *space = new QWidget;
                space->setFixedWidth(26);
                r->addWidget(space);
            }
            v->addWidget(card);
        }
        layout->addWidget(rows);
    };
    group(QStringLiteral("电脑"), "computers", computers);
    group(QStringLiteral("手机 / 平板"), "mobile", mobiles);
    if (!m_count) {
        auto *empty = label(QStringLiteral("暂无收藏设备"), "empty");
        empty->setText(favorites ? QStringLiteral("暂无收藏设备") : QStringLiteral("没有符合条件的设备"));
        empty->setObjectName("listEmpty");
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty, 1);
    } else
        layout->addStretch();
    auto *old = takeWidget();
    setWidget(content);
    if (old) {
        old->hide();
        old->deleteLater();
    }
}
} // namespace Ui
