#pragma once

#include <cstdint>
#include <vector>

namespace easydesk::media {

struct CaptureRect {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct CaptureGeometry {
    CaptureRect region;
    uint64_t generation = 0;
    bool valid = false;
};

class CaptureGeometryTracker final {
  public:
    CaptureGeometry update(int32_t frameX, int32_t frameY, uint32_t frameWidth,
                           uint32_t frameHeight, int32_t virtualX, int32_t virtualY,
                           uint32_t cropX, uint32_t cropY, uint32_t cropWidth,
                           uint32_t cropHeight, const std::vector<CaptureRect> &monitors,
                           bool physicalCoordinates);
    CaptureGeometry invalidate();

  private:
    CaptureGeometry set(const CaptureRect &region, bool valid);
    CaptureGeometry m_geometry;
};

}  // namespace easydesk::media
