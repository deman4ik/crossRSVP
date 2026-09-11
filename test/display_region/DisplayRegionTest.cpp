#include <DisplayRegion.h>
#include <gtest/gtest.h>

#include <limits>

namespace {

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

}  // namespace
