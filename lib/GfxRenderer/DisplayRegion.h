#pragma once

#include <stdint.h>

namespace display_region {

// Half-open logical rectangle in the renderer's current orientation.
struct LogicalRegion {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
};

// Byte-addressable rectangle in physical framebuffer coordinates.
struct PhysicalRegion {
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t width = 0;
  uint16_t height = 0;

  constexpr bool valid() const { return width != 0 && height != 0 && (x & 0x7u) == 0 && (width & 0x7u) == 0; }
};

enum class Orientation : uint8_t { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

// Rotates a logical rectangle to panel memory, expands physical X outwards to
// byte boundaries, then clips it to the physical panel. Invalid, empty, or
// fully off-panel input returns an empty region.
PhysicalRegion toPhysical(LogicalRegion logical, Orientation orientation, uint16_t panelWidth, uint16_t panelHeight);

}  // namespace display_region
