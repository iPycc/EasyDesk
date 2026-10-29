#include "ui/windows/main_window.h"
#include "app/application_context.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPalette>

static_assert(QT_VERSION == QT_VERSION_CHECK(5, 15, 12), "Build with Qt 5.15.12");
int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("EasyDesk");
    app.setOrganizationName("EasyDesk");
    app.setApplicationVersion("0.2.0-dev");
    Ui::installStyle();
    QPalette palette = app.palette();
    palette.setColor(QPalette::Highlight, QColor("#dfeafb"));
    palette.setColor(QPalette::HighlightedText, QColor("#377bcf"));
    app.setPalette(palette);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    const auto families = QFontDatabase().families();
    for (const QString &family :
         QStringList{QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")}) {
        if (families.contains(family)) {
            font.setFamily(family);
            break;
        }
    }
    font.setPixelSize(14);
    font.setWeight(QFont::Normal);
    app.setFont(font);
    Ui::applyTheme(false);
    ApplicationContext services;
    MainWindow window(services.devices(), services.sessions(), services.transfers(), services.localSystem(),
                      services.settings());
    window.show();
    return app.exec();
}
