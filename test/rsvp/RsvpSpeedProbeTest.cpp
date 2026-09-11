#include <gtest/gtest.h>

#include "RsvpSpeedProbe.h"

TEST(RsvpSpeedProbe, ExcludesDuplicateRedraws) {
  rsvp::RsvpSpeedProbe probe;

  EXPECT_TRUE(probe.record(1, 100, 1000, 400, false));
  EXPECT_FALSE(probe.record(1, 100, 9000, 999, false));
  EXPECT_TRUE(probe.record(2, 100, 2000, 500, false));

  EXPECT_EQ(probe.sampleCount(), 2u);
  EXPECT_EQ(probe.intervalMs(), 1000u);
  EXPECT_EQ(probe.averageFastMs(), 450u);
}

TEST(RsvpSpeedProbe, IntervalGetterReturnsLatestRawInterval) {
  rsvp::RsvpSpeedProbe probe;
  ASSERT_TRUE(probe.record(1, 100, 1000, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 1300, 400, false));
  EXPECT_EQ(probe.intervalMs(), 300u);
  ASSERT_TRUE(probe.record(3, 100, 3000, 400, false));
  EXPECT_EQ(probe.intervalMs(), 1700u);
}

TEST(RsvpSpeedProbe, InterruptStartsFreshRunWithoutCountingIdleGap) {
  rsvp::RsvpSpeedProbe probe;
  ASSERT_TRUE(probe.record(1, 100, 1000, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 2000, 400, false));

  probe.interrupt();
  EXPECT_EQ(probe.actualFramesPerMinute(), 60u);
  EXPECT_FALSE(probe.record(2, 100, 10000, 400, false));
  ASSERT_TRUE(probe.record(3, 100, 10000, 400, false));
  EXPECT_EQ(probe.run(), 2u);
  EXPECT_EQ(probe.sampleCount(), 1u);
  EXPECT_EQ(probe.actualFramesPerMinute(), 0u);
  ASSERT_TRUE(probe.record(4, 100, 11000, 400, false));
  EXPECT_EQ(probe.intervalMs(), 1000u);
}

TEST(RsvpSpeedProbe, PaceChangeResetsBeforeNewBaseline) {
  rsvp::RsvpSpeedProbe probe;
  ASSERT_TRUE(probe.record(1, 100, 1000, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 2000, 400, false));

  ASSERT_TRUE(probe.record(3, 125, 9000, 450, false));
  EXPECT_EQ(probe.run(), 2u);
  EXPECT_EQ(probe.sampleCount(), 1u);
  ASSERT_TRUE(probe.record(4, 125, 10000, 450, false));
  EXPECT_EQ(probe.intervalMs(), 1000u);
}

TEST(RsvpSpeedProbe, CleanupFramesAffectCadenceButNotFastMean) {
  rsvp::RsvpSpeedProbe probe;
  ASSERT_TRUE(probe.record(1, 100, 1000, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 2000, 600, true));
  ASSERT_TRUE(probe.record(3, 100, 4000, 500, false));

  EXPECT_EQ(probe.sampleCount(), 3u);
  EXPECT_EQ(probe.intervalMs(), 2000u);
  EXPECT_EQ(probe.actualFramesPerMinute(), 40u);
  EXPECT_EQ(probe.averageFastMs(), 450u);
}

TEST(RsvpSpeedProbe, UsesUnsignedMillisDifferenceAcrossRollover) {
  rsvp::RsvpSpeedProbe probe;
  ASSERT_TRUE(probe.record(1, 100, UINT32_MAX - 99u, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 50u, 400, false));

  EXPECT_EQ(probe.sampleCount(), 2u);
  EXPECT_EQ(probe.intervalMs(), 150u);
}

TEST(RsvpSpeedProbe, EmptyAndZeroIntervalsDoNotDivideByZero) {
  rsvp::RsvpSpeedProbe probe;
  EXPECT_EQ(probe.intervalMs(), 0u);
  EXPECT_EQ(probe.actualFramesPerMinute(), 0u);
  EXPECT_EQ(probe.averageFastMs(), 0u);
  ASSERT_TRUE(probe.record(1, 100, 500, 400, false));
  ASSERT_TRUE(probe.record(2, 100, 500, 400, false));
  EXPECT_EQ(probe.intervalMs(), 0u);
  EXPECT_EQ(probe.actualFramesPerMinute(), 0u);
}

TEST(RsvpSpeedProbe, ExposesDiagnosticPaceConstantsAsU16) {
  static_assert(rsvp::RsvpSpeedProbe::MAXIMUM_WPM == 600);
  EXPECT_EQ(sizeof(rsvp::RsvpSpeedProbe::MAXIMUM_WPM), sizeof(uint16_t));
  EXPECT_EQ(rsvp::RsvpSpeedProbe::MAXIMUM_WPM, 600u);
}
