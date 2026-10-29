#include "ui/ui.h"
#include "ui/components/icons.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace Ui {

namespace {
bool darkMode = false;
class ControlStyle : public QProxyStyle {
  public:
    ControlStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
    void polish(QWidget *widget) override {
        QProxyStyle::polish(widget);
        if (qobject_cast<QMenu *>(widget) && !widget->isVisible()) {
            widget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
            widget->setAttribute(Qt::WA_TranslucentBackground);
        }
    }
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override {
        if (element == PE_FrameFocusRect)
            return;
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};
} // namespace
void installStyle() {
    qApp->setStyle(new ControlStyle);
}
bool isDark() {
    return darkMode;
}
QColor accent() {
    return QColor("#3478ed");
}
void applyTheme(bool dark) {
    darkMode = dark;
    QPalette p;
    p.setColor(QPalette::Window, QColor(dark ? "#202020" : "#eff4f8"));
    p.setColor(QPalette::WindowText, QColor(dark ? "#f1f1f1" : "#202020"));
    p.setColor(QPalette::Text, p.color(QPalette::WindowText));
    p.setColor(QPalette::ButtonText, p.color(QPalette::WindowText));
    p.setColor(QPalette::Base, QColor(dark ? "#2d2d2d" : "#fdfefe"));
    p.setColor(QPalette::Button, p.color(QPalette::Base));
    p.setColor(QPalette::Highlight, accent());
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(dark ? "#777777" : "#a1a1a1"));
    p.setColor(QPalette::Disabled, QPalette::Text, p.color(QPalette::Disabled, QPalette::WindowText));
    p.setColor(QPalette::PlaceholderText, QColor(dark ? "#999999" : "#8a8a8a"));
    qApp->setPalette(p);
    QFile file(":/style.qss");
    file.open(QIODevice::ReadOnly);
    QString qss = QString::fromUtf8(file.readAll());
    const QMap<QString, QString> colors = {{"CONTENT", dark ? "#292929" : "#f8fbfe"},
                                           {"BG", dark ? "#202020" : "#eff4f8"},
                                           {"SURFACE", dark ? "#2b2b2b" : "#fdfefe"},
                                           {"SURFACE_HOVER", dark ? "#323232" : "#f4f8fc"},
                                           {"FIELD", dark ? "#343434" : "#fdfefe"},
                                           {"TEXT", dark ? "#f1f1f1" : "#202020"},
                                           {"MUTED", dark ? "#b6b6b6" : "#666666"},
                                           {"BORDER", dark ? "#3b3b3b" : "#e0e6ed"},
                                           {"HOVER", dark ? "#2d2d2d" : "#e5ecf3"},
                                           {"SELECTED", dark ? "#333333" : "#dfe8f2"},
                                           {"ACCENT", "#3478ed"},
                                           {"LINK", dark ? "#91b9ff" : "#2466d4"},
                                           {"DISABLED", dark ? "#777777" : "#aaaaaa"},
                                           {"SCROLL", dark ? "#666666" : "#b9b9b9"},
                                           {"CHEVRON", dark ? "dark" : "light"},
                                           {"CHECK", dark ? "check-white" : "check"},
                                           {"SUCCESS", dark ? "#91e3b6" : "#116b43"},
                                           {"SUCCESS_BG", dark ? "#164c37" : "#c5ecd7"}};
    for (auto i = colors.cbegin(); i != colors.cend(); ++i)
        qss.replace("@" + i.key() + "@", i.value());
    qApp->setStyleSheet(qss);
    for (auto *w : qApp->allWidgets()) {
        if (auto *l = qobject_cast<QLabel *>(w); l && !l->property("glyph").toString().isEmpty())
            l->setPixmap(icon(l->property("glyph").toString()).pixmap(22, 22));
        w->update();
    }
#ifdef Q_OS_WIN
    for (auto *w : qApp->topLevelWidgets()) {
        BOOL value = dark;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(w->winId()), 20, &value, sizeof(value));
    }
#endif
}
} // namespace Ui
