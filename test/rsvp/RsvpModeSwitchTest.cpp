#include <gtest/gtest.h>

#include "RsvpModeSwitch.h"

namespace {

rsvp::ResumeAnchor anchor(const uint16_t spineIndex, const uint32_t offset, const uint16_t ordinal = 0) {
  return {.spineIndex = spineIndex, .visibleTextOffset = offset, .sameOffsetOrdinal = ordinal, .valid = true};
}

TEST(RsvpModeSwitch, NewlyOpenedBookStartsInPagedModeWithoutAnAnchor) {
  const auto decision = rsvp::RsvpModeSwitch::newBook();

  EXPECT_EQ(decision.mode, rsvp::ReadingMode::Paged);
  EXPECT_FALSE(decision.anchor.valid);
  EXPECT_FALSE(decision.temporaryHighlight);
  EXPECT_FALSE(decision.paused);
}

TEST(RsvpModeSwitch, LeavingRsvpHighlightsTheLastDisplayedAnchorInPagedMode) {
  const auto lastDisplayed = anchor(2, 417, 1);

  const auto decision = rsvp::RsvpModeSwitch::fromRsvp(lastDisplayed);

  EXPECT_EQ(decision.mode, rsvp::ReadingMode::Paged);
  EXPECT_EQ(decision.anchor.spineIndex, 2);
  EXPECT_EQ(decision.anchor.visibleTextOffset, 417u);
  EXPECT_EQ(decision.anchor.sameOffsetOrdinal, 1u);
  EXPECT_TRUE(decision.anchor.valid);
  EXPECT_TRUE(decision.temporaryHighlight);
  EXPECT_FALSE(decision.paused);
}

TEST(RsvpModeSwitch, ReturningToRsvpOnTheSamePageRepeatsTheCurrentAnchor) {
  const rsvp::PagedResumeContext context{.currentAnchor = anchor(1, 80, 2),
                                         .pageStartAnchor = anchor(1, 64),
                                         .pageTurned = false};

  const auto decision = rsvp::RsvpModeSwitch::fromPaged(context);

  EXPECT_EQ(decision.mode, rsvp::ReadingMode::Rsvp);
  EXPECT_EQ(decision.anchor.spineIndex, 1);
  EXPECT_EQ(decision.anchor.visibleTextOffset, 80u);
  EXPECT_EQ(decision.anchor.sameOffsetOrdinal, 2u);
  EXPECT_TRUE(decision.anchor.valid);
  EXPECT_TRUE(decision.paused);
  EXPECT_FALSE(decision.temporaryHighlight);
}

TEST(RsvpModeSwitch, ReturningToRsvpAfterPageTurnStartsAtTheDisplayedPageStart) {
  const rsvp::PagedResumeContext context{.currentAnchor = anchor(1, 80, 2),
                                         .pageStartAnchor = anchor(1, 128, 3),
                                         .pageTurned = true};

  const auto decision = rsvp::RsvpModeSwitch::fromPaged(context);

  EXPECT_EQ(decision.mode, rsvp::ReadingMode::Rsvp);
  EXPECT_EQ(decision.anchor.spineIndex, 1);
  EXPECT_EQ(decision.anchor.visibleTextOffset, 128u);
  EXPECT_EQ(decision.anchor.sameOffsetOrdinal, 3u);
  EXPECT_TRUE(decision.anchor.valid);
  EXPECT_TRUE(decision.paused);
  EXPECT_FALSE(decision.temporaryHighlight);
}

}  // namespace
