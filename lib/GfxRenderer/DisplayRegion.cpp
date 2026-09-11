#include "DisplayRegion.h"

#include <algorithm>
#include <limits>

namespace display_region {
namespace {

int64_t floorToByte(const int64_t value) { return value >= 0 ? (value / 8) * 8 : -(((-value + 7) / 8) * 8); }

int64_t ceilToByte(const int64_t value) { return value >= 0 ? ((value + 7) / 8) * 8 : -((-value) / 8) * 8; }

}  // namespace

PhysicalRegion toPhysical(const LogicalRegion logical, const Orientation orientation, const uint16_t panelWidth,
                          const uint16_t panelHeight) {
  PhysicalRegion result;
  if (logical.width <= 0 || logical.height <= 0 || panelWidth == 0 || panelHeight == 0 || (panelWidth & 0x7u) != 0) {
    return result;
  }

  const int64_t x0 = logical.x;
  const int64_t y0 = logical.y;
  const int64_t x1 = x0 + logical.width;
  const int64_t y1 = y0 + logical.height;
  int64_t physicalX0 = 0;
  int64_t physicalY0 = 0;
  int64_t physicalX1 = 0;
  int64_t physicalY1 = 0;

  switch (orientation) {
    case Orientation::Portrait:
      physicalX0 = y0;
      physicalX1 = y1;
      physicalY0 = static_cast<int64_t>(panelHeight) - x1;
      physicalY1 = static_cast<int64_t>(panelHeight) - x0;
      break;
    case Orientation::LandscapeClockwise:
      physicalX0 = static_cast<int64_t>(panelWidth) - x1;
      physicalX1 = static_cast<int64_t>(panelWidth) - x0;
      physicalY0 = static_cast<int64_t>(panelHeight) - y1;
      physicalY1 = static_cast<int64_t>(panelHeight) - y0;
      break;
    case Orientation::PortraitInverted:
      physicalX0 = static_cast<int64_t>(panelWidth) - y1;
      physicalX1 = static_cast<int64_t>(panelWidth) - y0;
      physicalY0 = x0;
      physicalY1 = x1;
      break;
    case Orientation::LandscapeCounterClockwise:
      physicalX0 = x0;
      physicalX1 = x1;
      physicalY0 = y0;
      physicalY1 = y1;
      break;
  }

  physicalX0 = floorToByte(physicalX0);
  physicalX1 = ceilToByte(physicalX1);
  physicalX0 = std::clamp<int64_t>(physicalX0, 0, panelWidth);
  physicalX1 = std::clamp<int64_t>(physicalX1, 0, panelWidth);
  physicalY0 = std::clamp<int64_t>(physicalY0, 0, panelHeight);
  physicalY1 = std::clamp<int64_t>(physicalY1, 0, panelHeight);
  if (physicalX1 <= physicalX0 || physicalY1 <= physicalY0 || physicalX1 > std::numeric_limits<uint16_t>::max() ||
      physicalY1 > std::numeric_limits<uint16_t>::max()) {
    return result;
  }

  result.x = static_cast<uint16_t>(physicalX0);
  result.y = static_cast<uint16_t>(physicalY0);
  result.width = static_cast<uint16_t>(physicalX1 - physicalX0);
  result.height = static_cast<uint16_t>(physicalY1 - physicalY0);
  return result;
}

}  // namespace display_region
