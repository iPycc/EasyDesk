#include "ui/input/video_input_mapping.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 kMaximumCoordinate = 65535;

bool finitePositive(double value) {
    return std::isfinite(value) && value > 0;
}

int normalizedCoordinate(float position, float start, float extent, int remotePixels) {
    const double fraction = static_cast<double>(position - start) / extent;
    const qint64 pixel = std::min<qint64>(remotePixels - 1,
        static_cast<qint64>(std::floor(fraction * remotePixels)));
    return remotePixels == 1 ? 0 : static_cast<int>(pixel * kMaximumCoordinate / (remotePixels - 1));
}
}  // namespace

bool mapVideoPointToRemote(const QPointF &logicalPoint, const QSizeF &logicalTarget,
                           const QSize &physicalTarget, const QSize &presentedFrame,
                           const QSize &remoteRegion, QPoint &normalized) {
    if (!finitePositive(logicalTarget.width()) || !finitePositive(logicalTarget.height()) ||
        !std::isfinite(logicalPoint.x()) || !std::isfinite(logicalPoint.y()) ||
        logicalPoint.x() < 0 || logicalPoint.y() < 0 ||
        logicalPoint.x() >= logicalTarget.width() || logicalPoint.y() >= logicalTarget.height() ||
        physicalTarget.width() <= 0 || physicalTarget.height() <= 0 ||
        presentedFrame.width() <= 0 || presentedFrame.height() <= 0 ||
        remoteRegion.width() <= 0 || remoteRegion.height() <= 0)
        return false;

    const float width = static_cast<float>(physicalTarget.width());
    const float height = static_cast<float>(physicalTarget.height());
    const float sourceAspect = static_cast<float>(presentedFrame.width()) / presentedFrame.height();
    const float targetAspect = width / height;
    float left = 0, top = 0, viewportWidth = width, viewportHeight = height;
    if (sourceAspect > targetAspect) {
        viewportHeight = width / sourceAspect;
        top = (height - viewportHeight) * 0.5f;
    } else {
        viewportWidth = height * sourceAspect;
        left = (width - viewportWidth) * 0.5f;
    }
    const float x = static_cast<float>(std::floor((logicalPoint.x() / logicalTarget.width()) *
                                                   physicalTarget.width())) + 0.5f;
    const float y = static_cast<float>(std::floor((logicalPoint.y() / logicalTarget.height()) *
                                                   physicalTarget.height())) + 0.5f;
    if (x < left || y < top || x >= left + viewportWidth || y >= top + viewportHeight)
        return false;

    normalized = QPoint(normalizedCoordinate(x, left, viewportWidth, remoteRegion.width()),
                        normalizedCoordinate(y, top, viewportHeight, remoteRegion.height()));
    return true;
}
