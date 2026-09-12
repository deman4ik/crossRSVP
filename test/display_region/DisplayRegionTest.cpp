#include <DisplayRegion.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <limits>

namespace {

using display_region::inkBounds;
using display_region::LogicalRegion;
using display_region::Orientation;
using display_region::PhysicalRegion;
using display_region::toPhysical;

constexpr uint16_t PANEL_WIDTH = 792;
constexpr uint16_t PANEL_HEIGHT = 528;

void expectRegion(const PhysicalRegion actual, const uint16_t x, const uint16_t y, const uint16_t width,
                  const uint16_t height) {
  EXPECT_TRUE(actual.valid());
  EXPECT_EQ(actual.x, x);
  EXPECT_EQ(actual.y, y);
  EXPECT_EQ(actual.width, width);
  EXPECT_EQ(actual.height, height);
}

TEST(DisplayRegion, ConvertsAndByteAlignsEveryOrientation) {
  constexpr LogicalRegion logical{13, 101, 37, 21};

  expectRegion(toPhysical(logical, Orientation::Portrait, PANEL_WIDTH, PANEL_HEIGHT), 96, 478, 32, 37);
  expectRegion(toPhysical(logical, Orientation::LandscapeClockwise, PANEL_WIDTH, PANEL_HEIGHT), 736, 406, 48, 21);
  expectRegion(toPhysical(logical, Orientation::PortraitInverted, PANEL_WIDTH, PANEL_HEIGHT), 664, 13, 32, 37);
  expectRegion(toPhysical(logical, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT), 8, 101, 48, 21);
}

TEST(DisplayRegion, ClipsAfterExpandingPhysicalXOutward) {
  const PhysicalRegion region =
      toPhysical({-3, -4, 11, 10}, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT);

  expectRegion(region, 0, 0, 8, 6);
}

TEST(DisplayRegion, ClipsAtTheFarPanelEdges) {
  const PhysicalRegion region =
      toPhysical({787, 523, 10, 10}, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT);

  expectRegion(region, 784, 523, 8, 5);
}

TEST(DisplayRegion, RejectsEmptyAndFullyOffPanelInput) {
  EXPECT_FALSE(toPhysical({0, 0, 0, 10}, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT).valid());
  EXPECT_FALSE(toPhysical({0, 0, 10, -1}, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT).valid());
  EXPECT_FALSE(toPhysical({900, 0, 10, 10}, Orientation::LandscapeCounterClockwise, PANEL_WIDTH, PANEL_HEIGHT).valid());
  EXPECT_FALSE(toPhysical({std::numeric_limits<int32_t>::max(), 0, 20, 20}, Orientation::LandscapeCounterClockwise,
                          PANEL_WIDTH, PANEL_HEIGHT)
                   .valid());
}

TEST(DisplayRegion, RejectsPanelWidthsThatCannotDescribeByteAlignedRows) {
  EXPECT_FALSE(toPhysical({0, 0, 10, 10}, Orientation::LandscapeCounterClockwise, 791, PANEL_HEIGHT).valid());
}

void setBlack(uint8_t* framebuffer, const uint16_t strideBytes, const uint16_t x, const uint16_t y) {
  framebuffer[static_cast<size_t>(y) * strideBytes + x / 8u] &= static_cast<uint8_t>(~(0x80u >> (x & 0x7u)));
}

TEST(DisplayRegion, InkBoundsMapsEveryOrientation) {
  constexpr uint16_t width = 32;
  constexpr uint16_t height = 16;
  constexpr uint16_t stride = 4;
  uint8_t framebuffer[height * stride];
  std::fill(std::begin(framebuffer), std::end(framebuffer), 0xFF);
  setBlack(framebuffer, stride, 3, 4);
  setBlack(framebuffer, stride, 19, 12);

  const LogicalRegion portrait =
      inkBounds(framebuffer, width, height, stride, {0, 0, height, width}, Orientation::Portrait);
  EXPECT_EQ(portrait.x, 3);
  EXPECT_EQ(portrait.y, 3);
  EXPECT_EQ(portrait.width, 9);
  EXPECT_EQ(portrait.height, 17);

  const LogicalRegion portraitInverted =
      inkBounds(framebuffer, width, height, stride, {0, 0, height, width}, Orientation::PortraitInverted);
  EXPECT_EQ(portraitInverted.x, 4);
  EXPECT_EQ(portraitInverted.y, 12);
  EXPECT_EQ(portraitInverted.width, 9);
  EXPECT_EQ(portraitInverted.height, 17);

  const LogicalRegion landscapeClockwise =
      inkBounds(framebuffer, width, height, stride, {0, 0, width, height}, Orientation::LandscapeClockwise);
  EXPECT_EQ(landscapeClockwise.x, 12);
  EXPECT_EQ(landscapeClockwise.y, 3);
  EXPECT_EQ(landscapeClockwise.width, 17);
  EXPECT_EQ(landscapeClockwise.height, 9);

  const LogicalRegion landscapeCounterClockwise =
      inkBounds(framebuffer, width, height, stride, {0, 0, width, height}, Orientation::LandscapeCounterClockwise);
  EXPECT_EQ(landscapeCounterClockwise.x, 3);
  EXPECT_EQ(landscapeCounterClockwise.y, 4);
  EXPECT_EQ(landscapeCounterClockwise.width, 17);
  EXPECT_EQ(landscapeCounterClockwise.height, 9);
}

TEST(DisplayRegion, InkBoundsUsesStrideAndClipsAlignedPhysicalRoi) {
  constexpr uint16_t width = 32;
  constexpr uint16_t height = 16;
  constexpr uint16_t stride = 6;
  uint8_t framebuffer[height * stride];
  std::fill(std::begin(framebuffer), std::end(framebuffer), 0xFF);
  setBlack(framebuffer, stride, 2, 5);
  setBlack(framebuffer, stride, 7, 5);
  setBlack(framebuffer, stride, 10, 5);  // Outside the byte-aligned ROI.

  const LogicalRegion result =
      inkBounds(framebuffer, width, height, stride, {3, 5, 5, 1}, Orientation::LandscapeCounterClockwise);
  EXPECT_EQ(result.x, 2);  // The scan includes physical x=0..7, then returns exact ink bounds.
  EXPECT_EQ(result.y, 5);
  EXPECT_EQ(result.width, 6);
  EXPECT_EQ(result.height, 1);
}

TEST(DisplayRegion, InkBoundsReturnsEmptyForNullInvalidOrWhiteInput) {
  constexpr uint8_t white[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(inkBounds(nullptr, 16, 4, 2, {0, 0, 16, 4}, Orientation::LandscapeCounterClockwise).width, 0);
  EXPECT_EQ(inkBounds(white, 16, 4, 1, {0, 0, 16, 4}, Orientation::LandscapeCounterClockwise).height, 0);
  EXPECT_EQ(inkBounds(white, 16, 4, 2, {0, 0, 16, 4}, Orientation::LandscapeCounterClockwise).width, 0);
  EXPECT_EQ(inkBounds(white, 16, 4, 2, {0, 0, 0, 4}, Orientation::LandscapeCounterClockwise).height, 0);
}

TEST(DisplayRegion, InkBoundsIncludesPhysicalEdgePixels) {
  constexpr uint16_t width = 16;
  constexpr uint16_t height = 8;
  constexpr uint16_t stride = 2;
  uint8_t framebuffer[height * stride];
  std::fill(std::begin(framebuffer), std::end(framebuffer), 0xFF);
  setBlack(framebuffer, stride, 0, 0);
  setBlack(framebuffer, stride, width - 1, height - 1);

  const LogicalRegion result =
      inkBounds(framebuffer, width, height, stride, {0, 0, width, height}, Orientation::LandscapeCounterClockwise);
  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.y, 0);
  EXPECT_EQ(result.width, width);
  EXPECT_EQ(result.height, height);
}

}  // namespace
