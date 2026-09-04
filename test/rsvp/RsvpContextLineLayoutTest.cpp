#include <gtest/gtest.h>

#include "RsvpWordLayout.h"

namespace {

rsvp::ContextLineInput baseInput() {
  rsvp::ContextLineInput input;
  input.focusX = 100;
  input.leftBound = 0;
  input.rightBound = 200;
  input.gap = 4;
  input.fontSize = 28;
  input.prefixAdvance = 20;
  input.pivotAdvance = 10;
  input.suffixAdvance = 30;
  return input;
}

TEST(RsvpContextLineLayout, KeepsActiveOrpAtFixedFocusAndUsesConfiguredFontSize) {
  const auto input = baseInput();
  rsvp::ContextLineLayout output;

  ASSERT_TRUE(rsvp::calculateRsvpContextLineLayout(input, output));

  EXPECT_EQ(output.active.pivotX + (output.active.pivotAdvance + 1) / 2, input.focusX);
  EXPECT_EQ(output.active.prefixAdvance, input.prefixAdvance);
  EXPECT_EQ(output.active.pivotAdvance, input.pivotAdvance);
  EXPECT_EQ(output.active.suffixAdvance, input.suffixAdvance);
  EXPECT_EQ(output.fontSize, input.fontSize);
}

TEST(RsvpContextLineLayout, FillsLeftAndRightIndependentlyNearestFirst) {
  auto input = baseInput();
  input.leftNearest[0] = {.advance = 68};
  input.leftNearest[1] = {.advance = 10};
  input.leftCount = 2;
  input.rightNearest[0] = {.advance = 10};
  input.rightNearest[1] = {.advance = 10};
  input.rightCount = 2;
  rsvp::ContextLineLayout output;

  ASSERT_TRUE(rsvp::calculateRsvpContextLineLayout(input, output));

  EXPECT_TRUE(output.leftVisible[0]);
  EXPECT_FALSE(output.leftVisible[1]);
  EXPECT_TRUE(output.rightVisible[0]);
  EXPECT_TRUE(output.rightVisible[1]);
  EXPECT_LT(output.leftX[0], output.active.startX);
  EXPECT_GT(output.rightX[0], output.active.suffixX);
}

TEST(RsvpContextLineLayout, OmitsWholeNearestTokenInsteadOfClippingOrSkippingAhead) {
  auto input = baseInput();
  input.leftNearest[0] = {.advance = 80};
  input.leftNearest[1] = {.advance = 10};
  input.leftCount = 2;
  rsvp::ContextLineLayout output;

  ASSERT_TRUE(rsvp::calculateRsvpContextLineLayout(input, output));

  EXPECT_FALSE(output.leftVisible[0]);
  EXPECT_FALSE(output.leftVisible[1]);
}

TEST(RsvpContextLineLayout, RetainsStandalonePunctuationAsAWholeContextToken) {
  auto input = baseInput();
  input.rightNearest[0] = {.advance = 8, .punctuation = true};
  input.rightNearest[1] = {.advance = 30};
  input.rightCount = 2;
  rsvp::ContextLineLayout output;

  ASSERT_TRUE(rsvp::calculateRsvpContextLineLayout(input, output));

  EXPECT_TRUE(output.rightVisible[0]);
  EXPECT_TRUE(output.rightVisible[1]);
}

TEST(RsvpContextLineLayout, OmitsContextBeforeShrinkingTheActiveWord) {
  auto input = baseInput();
  input.leftNearest[0] = {.advance = 100};
  input.leftCount = 1;
  input.rightNearest[0] = {.advance = 100};
  input.rightCount = 1;
  rsvp::ContextLineLayout output;

  ASSERT_TRUE(rsvp::calculateRsvpContextLineLayout(input, output));

  EXPECT_EQ(output.fontSize, input.fontSize);
  EXPECT_EQ(output.active.prefixAdvance, input.prefixAdvance);
  EXPECT_EQ(output.active.pivotAdvance, input.pivotAdvance);
  EXPECT_EQ(output.active.suffixAdvance, input.suffixAdvance);
  EXPECT_FALSE(output.leftVisible[0]);
  EXPECT_FALSE(output.rightVisible[0]);
}

}  // namespace
