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

LogicalRegion inkBounds(const uint8_t* framebuffer, const uint16_t panelWidth, const uint16_t panelHeight,
                        const uint16_t strideBytes, const LogicalRegion safeLogical, const Orientation orientation) {
  if (framebuffer == nullptr || panelWidth == 0 || panelHeight == 0 || (panelWidth & 0x7u) != 0) return {};
  const uint32_t minimumStride = (static_cast<uint32_t>(panelWidth) + 7u) / 8u;
  if (strideBytes < minimumStride) return {};

  const PhysicalRegion safePhysical = toPhysical(safeLogical, orientation, panelWidth, panelHeight);
  if (!safePhysical.valid()) return {};

  uint16_t minPhysicalX = 0;
  uint16_t maxPhysicalX = 0;
  uint16_t minPhysicalY = 0;
  uint16_t maxPhysicalY = 0;
  bool foundInk = false;
  const uint32_t physicalYEnd = static_cast<uint32_t>(safePhysical.y) + safePhysical.height;
  const uint32_t physicalXEnd = static_cast<uint32_t>(safePhysical.x) + safePhysical.width;
  for (uint32_t physicalY = safePhysical.y; physicalY < physicalYEnd; ++physicalY) {
    const uint8_t* row = framebuffer + physicalY * strideBytes;
    for (uint32_t physicalX = safePhysical.x; physicalX < physicalXEnd; ++physicalX) {
      const uint8_t byte = row[physicalX / 8u];
      const uint8_t mask = static_cast<uint8_t>(0x80u >> (physicalX & 0x7u));
      if ((byte & mask) != 0) continue;
      const auto x = static_cast<uint16_t>(physicalX);
      const auto y = static_cast<uint16_t>(physicalY);
      if (!foundInk) {
        minPhysicalX = maxPhysicalX = x;
        minPhysicalY = maxPhysicalY = y;
        foundInk = true;
      } else {
        if (x < minPhysicalX) minPhysicalX = x;
        if (x > maxPhysicalX) maxPhysicalX = x;
        if (y < minPhysicalY) minPhysicalY = y;
        if (y > maxPhysicalY) maxPhysicalY = y;
      }
    }
  }
  if (!foundInk) return {};

  int32_t minLogicalX = 0;
  int32_t maxLogicalX = 0;
  int32_t minLogicalY = 0;
  int32_t maxLogicalY = 0;
  bool firstLogicalPoint = true;
  const auto includeLogicalPoint = [&](const uint16_t physicalX, const uint16_t physicalY) {
    int32_t logicalX = 0;
    int32_t logicalY = 0;
    switch (orientation) {
      case Orientation::Portrait:
        logicalX = static_cast<int32_t>(panelHeight) - 1 - physicalY;
        logicalY = physicalX;
        break;
      case Orientation::LandscapeClockwise:
        logicalX = static_cast<int32_t>(panelWidth) - 1 - physicalX;
        logicalY = static_cast<int32_t>(panelHeight) - 1 - physicalY;
        break;
      case Orientation::PortraitInverted:
        logicalX = physicalY;
        logicalY = static_cast<int32_t>(panelWidth) - 1 - physicalX;
        break;
      case Orientation::LandscapeCounterClockwise:
        logicalX = physicalX;
        logicalY = physicalY;
        break;
    }
    if (firstLogicalPoint) {
      minLogicalX = maxLogicalX = logicalX;
      minLogicalY = maxLogicalY = logicalY;
      firstLogicalPoint = false;
      return;
    }
    if (logicalX < minLogicalX) minLogicalX = logicalX;
    if (logicalX > maxLogicalX) maxLogicalX = logicalX;
    if (logicalY < minLogicalY) minLogicalY = logicalY;
    if (logicalY > maxLogicalY) maxLogicalY = logicalY;
  };
  includeLogicalPoint(minPhysicalX, minPhysicalY);
  includeLogicalPoint(maxPhysicalX, minPhysicalY);
  includeLogicalPoint(minPhysicalX, maxPhysicalY);
  includeLogicalPoint(maxPhysicalX, maxPhysicalY);

  return {minLogicalX, minLogicalY, maxLogicalX - minLogicalX + 1, maxLogicalY - minLogicalY + 1};
}

}  // namespace display_region
