#include <gtest/gtest.h>

#include "RsvpCompanionPolicy.h"

namespace {

using rsvp::CompanionRole;

TEST(RsvpCompanionPolicy, KeepsDeclaredLayersBounded) {
  EXPECT_EQ(rsvp::RSVP_COMPANION_CORE.size(), 50u);
  EXPECT_EQ(rsvp::RSVP_COMPANION_EXPERIMENTAL.size(), 8u);
}

TEST(RsvpCompanionPolicy, MatchesEveryCoreFormAndRole) {
  for (const auto& entry : rsvp::RSVP_COMPANION_CORE) {
    EXPECT_EQ(rsvp::classifyRsvpCompanion(entry.text), entry.role) << entry.text;
  }
}

TEST(RsvpCompanionPolicy, MatchesExperimentalPronounsSeparately) {
  for (const auto& entry : rsvp::RSVP_COMPANION_EXPERIMENTAL) {
    EXPECT_EQ(rsvp::classifyRsvpCompanion(entry.text), CompanionRole::Forward) << entry.text;
  }
}

TEST(RsvpCompanionPolicy, FoldsRussianCaseWithoutChangingYo) {
  EXPECT_EQ(rsvp::classifyRsvpCompanion("В"), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("ПЕРЕДО"), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("Ё"), CompanionRole::None);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("ЕЩЁ"), CompanionRole::None);
}

TEST(RsvpCompanionPolicy, IgnoresOnlySurroundingPunctuation) {
  EXPECT_EQ(rsvp::classifyRsvpCompanion("(в),"), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("же!"), CompanionRole::Backward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("из-за"), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("из—за"), CompanionRole::None);
}

TEST(RsvpCompanionPolicy, RejectsMeaningBearingShortForms) {
  static constexpr const char* DENY[] = {"я бы", "что", "это", "если", "да", "только", "меня", "мне", "есть", "надо"};
  for (const char* word : DENY) EXPECT_EQ(rsvp::classifyRsvpCompanion(word), CompanionRole::None) << word;
  EXPECT_EQ(rsvp::classifyRsvpCompanion("что,"), CompanionRole::None);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("если!"), CompanionRole::None);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("(меня);"), CompanionRole::None);
}

TEST(RsvpCompanionPolicy, ExposesGroupingBoundaries) {
  EXPECT_TRUE(rsvp::isRsvpGroupingBoundary(','));
  EXPECT_TRUE(rsvp::isRsvpGroupingBoundary(0x2014));
  EXPECT_TRUE(rsvp::isRsvpGroupingBoundary('?'));
  EXPECT_TRUE(rsvp::isRsvpGroupingBoundary('-'));
}

}  // namespace
