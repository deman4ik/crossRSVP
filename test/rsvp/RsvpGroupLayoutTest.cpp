#include <gtest/gtest.h>

#include "RsvpWordLayout.h"

namespace {
rsvp::GroupLayoutInput input() {
  rsvp::GroupLayoutInput value;
  value.focusX = 100;
  value.leftBound = 0;
  value.rightBound = 200;
  value.gap = 4;
  value.prefixAdvance = 20;
  value.pivotAdvance = 10;
  value.suffixAdvance = 30;
  return value;
}

TEST(RsvpGroupLayout, PreservesActiveOrpWithCompanionsOnBothSides) {
  auto value = input();
  value.count = 3;
  value.activeIndex = 1;
  value.advances = {12, 60, 18};
  rsvp::GroupLayout result;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.begin, 0);
  EXPECT_EQ(result.end, 3);
  EXPECT_EQ(result.active.pivotX + 5, 100);
  EXPECT_EQ(result.positions[0] + 12 + 4, result.active.startX);
  EXPECT_EQ(result.positions[2], result.active.startX + 60 + 4);
}

TEST(RsvpGroupLayout, RemovesFarthestPrefixFirstWithoutMovingActive) {
  auto value = input();
  value.count = 3;
  value.activeIndex = 2;
  value.advances = {50, 30, 60};
  rsvp::GroupLayout result;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.begin, 1);
  EXPECT_EQ(result.end, 3);
  EXPECT_EQ(result.active.startX, 75);
  EXPECT_EQ(result.active.totalWidth, 60);
}

TEST(RsvpGroupLayout, DoesNotSkipOversizedNearestCompanion) {
  auto value = input();
  value.count = 3;
  value.activeIndex = 2;
  value.advances = {1, 100, 60};
  rsvp::GroupLayout result;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.begin, 2);
  EXPECT_EQ(result.end, 3);
}

TEST(RsvpGroupLayout, FallsBackToActiveOnlyWithoutShrinking) {
  auto value = input();
  value.count = 3;
  value.activeIndex = 1;
  value.advances = {100, 60, 100};
  rsvp::GroupLayout result;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.begin, 1);
  EXPECT_EQ(result.end, 2);
  EXPECT_EQ(result.active.totalWidth, 60);
}

TEST(RsvpGroupLayout, RejectsInvalidMetricsOrActiveThatDoesNotFit) {
  auto value = input();
  rsvp::GroupLayout result;
  value.count = 4;
  EXPECT_FALSE(rsvp::calculateRsvpGroupLayout(value, result));
  value.count = 1;
  value.prefixAdvance = 101;
  EXPECT_FALSE(rsvp::calculateRsvpGroupLayout(value, result));
}

TEST(RsvpGroupLayout, HandlesTwoPostfixesAndExactBoundaryFit) {
  auto value = input();
  value.count = 3;
  value.advances = {60, 30, 30};
  rsvp::GroupLayout result;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.begin, 0);
  EXPECT_EQ(result.end, 2);
  value.advances[2] = 27;
  ASSERT_TRUE(rsvp::calculateRsvpGroupLayout(value, result));
  EXPECT_EQ(result.end, 3);
  EXPECT_EQ(result.positions[2] + 27, 200);
}
}  // namespace
