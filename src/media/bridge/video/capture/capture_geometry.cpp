#include "video/capture/capture_geometry.h"

#include <limits>

namespace easydesk::media {
namespace {
bool sameRect(const CaptureRect &a, const CaptureRect &b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

bool fitsInt32(int64_t value) {
    return value >= std::numeric_limits<int32_t>::min() &&
           value <= std::numeric_limits<int32_t>::max();
}
}  // namespace

CaptureGeometry CaptureGeometryTracker::update(
    int32_t frameX, int32_t frameY, uint32_t frameWidth, uint32_t frameHeight,
    int32_t virtualX, int32_t virtualY, uint32_t cropX, uint32_t cropY,
    uint32_t cropWidth, uint32_t cropHeight, const std::vector<CaptureRect> &monitors,
    bool physicalCoordinates) {
    const int64_t x = static_cast<int64_t>(virtualX) + frameX;
    const int64_t y = static_cast<int64_t>(virtualY) + frameY;
    if (!physicalCoordinates || frameX < 0 || frameY < 0 || frameWidth == 0 ||
        frameHeight == 0 || cropWidth == 0 || cropHeight == 0 ||
        cropX > frameWidth || cropWidth > frameWidth - cropX ||
        cropY > frameHeight || cropHeight > frameHeight - cropY ||
        !fitsInt32(x) || !fitsInt32(y) || !fitsInt32(x + frameWidth - 1) ||
        !fitsInt32(y + frameHeight - 1))
        return invalidate();

    const CaptureRect frame{static_cast<int32_t>(x), static_cast<int32_t>(y),
                            frameWidth, frameHeight};
    int matches = 0;
    for (const auto &monitor : monitors) {
        if (sameRect(frame, monitor))
            ++matches;
    }
    if (matches != 1)
        return invalidate();
    return set({static_cast<int32_t>(x + cropX), static_cast<int32_t>(y + cropY),
                cropWidth, cropHeight}, true);
}

CaptureGeometry CaptureGeometryTracker::invalidate() {
    return set({}, false);
}

CaptureGeometry CaptureGeometryTracker::set(const CaptureRect &region, bool valid) {
    if (m_geometry.generation == 0 || m_geometry.valid != valid ||
        !sameRect(m_geometry.region, region)) {
        if (m_geometry.generation == std::numeric_limits<uint64_t>::max()) {
            m_geometry.region = {};
            m_geometry.valid = false;
            return m_geometry;
        }
        ++m_geometry.generation;
        m_geometry.region = region;
        m_geometry.valid = valid;
    }
    return m_geometry;
}

}  // namespace easydesk::media
