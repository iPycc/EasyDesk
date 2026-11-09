#pragma once

#include <QPoint>
#include <QRect>

bool mapInputToVirtualDesktop(const QRect &display, const QRect &desktop,
                              quint32 x, quint32 y, QPoint &mapped);
