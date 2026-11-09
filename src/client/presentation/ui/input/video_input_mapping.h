#pragma once

#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSizeF>

// Target dimensions are the physical HWND client size. The frame dimensions
// must describe the frame currently presented by the renderer.
bool mapVideoPointToRemote(const QPointF &logicalPoint, const QSizeF &logicalTarget,
                           const QSize &physicalTarget, const QSize &presentedFrame,
                           const QSize &remoteRegion, QPoint &normalized);
