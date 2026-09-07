#include <gtest/gtest.h>

#include "RsvpPendingActions.h"

TEST(RsvpPendingActions, RetainsActionsAndReturnsThemByPriority) {
  rsvp::RsvpPendingActions pending;

  ASSERT_TRUE(pending.push(rsvp::Action::PaceUp));
  ASSERT_TRUE(pending.push(rsvp::Action::TogglePlayback));
  ASSERT_TRUE(pending.push(rsvp::Action::ModeSwitch));
  ASSERT_TRUE(pending.push(rsvp::Action::Exit));

  EXPECT_EQ(pending.pop(), rsvp::Action::Exit);
  EXPECT_EQ(pending.pop(), rsvp::Action::ModeSwitch);
  EXPECT_EQ(pending.pop(), rsvp::Action::TogglePlayback);
  EXPECT_EQ(pending.pop(), rsvp::Action::PaceUp);
  EXPECT_EQ(pending.pop(), rsvp::Action::None);
}

TEST(RsvpPendingActions, RetainsRepeatedActions) {
  rsvp::RsvpPendingActions pending;

  ASSERT_TRUE(pending.push(rsvp::Action::PaceUp));
  ASSERT_TRUE(pending.push(rsvp::Action::PaceUp));

  EXPECT_EQ(pending.pop(), rsvp::Action::PaceUp);
  EXPECT_EQ(pending.pop(), rsvp::Action::PaceUp);
  EXPECT_TRUE(pending.empty());
}

TEST(RsvpPendingActions, DiscardsOnlyMatchingActions) {
  rsvp::RsvpPendingActions pending;

  ASSERT_TRUE(pending.push(rsvp::Action::TogglePlayback));
  ASSERT_TRUE(pending.push(rsvp::Action::StepForward));
  ASSERT_TRUE(pending.push(rsvp::Action::TogglePlayback));
  ASSERT_TRUE(pending.push(rsvp::Action::PaceUp));
  ASSERT_TRUE(pending.push(rsvp::Action::Exit));

  EXPECT_EQ(pending.discard(rsvp::Action::TogglePlayback), 2U);
  EXPECT_EQ(pending.count(), 3U);
  EXPECT_EQ(pending.pop(), rsvp::Action::Exit);
  EXPECT_EQ(pending.pop(), rsvp::Action::PaceUp);
  EXPECT_EQ(pending.pop(), rsvp::Action::StepForward);
  EXPECT_TRUE(pending.empty());
}

TEST(RsvpPendingActions, ModalPauseDiscardPreservesOtherControls) {
  rsvp::RsvpPendingActions pending;

  ASSERT_TRUE(pending.push(rsvp::Action::TogglePlayback));
  ASSERT_TRUE(pending.push(rsvp::Action::StepForward));
  ASSERT_TRUE(pending.push(rsvp::Action::PaceDown));
  ASSERT_TRUE(pending.push(rsvp::Action::Exit));

  // A modal pause is applied directly to the session; a stale toggle must not
  // resume playback after the modal closes, while other controls remain queued.
  EXPECT_EQ(pending.discard(rsvp::Action::TogglePlayback), 1U);
  EXPECT_EQ(pending.count(), 3U);
  EXPECT_EQ(pending.pop(), rsvp::Action::Exit);
  EXPECT_EQ(pending.pop(), rsvp::Action::PaceDown);
  EXPECT_EQ(pending.pop(), rsvp::Action::StepForward);
  EXPECT_TRUE(pending.empty());
}

TEST(RsvpPendingActions, RejectsInvalidActions) {
  rsvp::RsvpPendingActions pending;

  EXPECT_FALSE(pending.push(rsvp::Action::None));
  EXPECT_FALSE(pending.push(rsvp::Action::FramePresented));
  EXPECT_TRUE(pending.empty());
}

TEST(RsvpPendingActions, RejectsOverflowWithoutDroppingBufferedActions) {
  rsvp::RsvpPendingActions pending;

  for (size_t index = 0; index < rsvp::RsvpPendingActions::CAPACITY; ++index) {
    ASSERT_TRUE(pending.push(rsvp::Action::StepForward));
  }
  EXPECT_FALSE(pending.push(rsvp::Action::TogglePlayback));
  EXPECT_EQ(pending.count(), rsvp::RsvpPendingActions::CAPACITY);
  for (size_t index = 0; index < rsvp::RsvpPendingActions::CAPACITY; ++index) {
    EXPECT_EQ(pending.pop(), rsvp::Action::StepForward);
  }
  EXPECT_TRUE(pending.empty());
}
