#pragma once
#include <QColor>
namespace Ui {
bool isDark();
void applyTheme(bool dark);
QColor accent();
void installStyle();
} // namespace Ui
