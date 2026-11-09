#include "input/coordinate_mapping.h"

namespace {
constexpr quint32 kMaximumCoordinate = 65535;
}

bool mapInputToVirtualDesktop(const QRect &display, const QRect &desktop,
                              quint32 x, quint32 y, QPoint &mapped) {
    if (x > kMaximumCoordinate || y > kMaximumCoordinate || display.width() <= 0 ||
        display.height() <= 0 || desktop.width() <= 0 || desktop.height() <= 0)
        return false;

    const qint64 desktopRight = static_cast<qint64>(desktop.x()) + desktop.width() - 1;
    const qint64 desktopBottom = static_cast<qint64>(desktop.y()) + desktop.height() - 1;
    const qint64 displayRight = static_cast<qint64>(display.x()) + display.width() - 1;
    const qint64 displayBottom = static_cast<qint64>(display.y()) + display.height() - 1;
    if (display.x() < desktop.x() || display.y() < desktop.y() ||
        displayRight > desktopRight || displayBottom > desktopBottom)
        return false;

    const qint64 pixelX = display.x() + static_cast<qint64>(display.width() - 1) * x / kMaximumCoordinate;
    const qint64 pixelY = display.y() + static_cast<qint64>(display.height() - 1) * y / kMaximumCoordinate;
    const int virtualX = desktop.width() == 1 ? 0 : static_cast<int>(
        (pixelX - desktop.x()) * kMaximumCoordinate / (desktop.width() - 1));
    const int virtualY = desktop.height() == 1 ? 0 : static_cast<int>(
        (pixelY - desktop.y()) * kMaximumCoordinate / (desktop.height() - 1));
    mapped = QPoint(virtualX, virtualY);
    return true;
}
