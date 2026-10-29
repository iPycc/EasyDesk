#include "ui/components/icons.h"
#include <QApplication>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>

namespace {
class SvgIconEngine : public QIconEngine {
  public:
    SvgIconEngine(const QString &name, const QColor &color) : m_name(name), m_color(color) {}
    QIconEngine *clone() const override {
        return new SvgIconEngine(m_name, m_color);
    }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
        QFile file(":/icons/" + m_name + ".svg");
        if (!file.open(QIODevice::ReadOnly))
            return;
        QColor color = m_color.isValid() ? m_color : qApp->palette().color(QPalette::WindowText);
        if (mode == QIcon::Disabled)
            color = qApp->palette().color(QPalette::Disabled, QPalette::WindowText);
        QByteArray data = file.readAll();
        data.replace("#60656f", color.name().toUtf8());
        QSvgRenderer renderer(data);
        renderer.render(p, rect);
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(), size), mode, state);
        return pm;
    }

  private:
    QString m_name;
    QColor m_color;
};
} // namespace
QIcon icon(const QString &name, const QColor &color, int size) {
    Q_UNUSED(size);
    return QIcon(new SvgIconEngine(name, color));
}

QString platformIcon(const QString &system) {
    if (system.contains("mac", Qt::CaseInsensitive) || system.contains("iOS", Qt::CaseInsensitive) ||
        system.contains("iPad", Qt::CaseInsensitive))
        return "apple";
    if (system.contains("linux", Qt::CaseInsensitive) || system.contains("ubuntu", Qt::CaseInsensitive))
        return "linux";
    return "windows";
}
