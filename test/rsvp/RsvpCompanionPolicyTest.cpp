#include <gtest/gtest.h>

#include <string>

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

TEST(RsvpCompanionPolicy, MatchesAllEnglishLayersAndApostrophes) {
  EXPECT_EQ(rsvp::RSVP_ENGLISH_COMPANION_CORE.size(), 14u);
  EXPECT_EQ(rsvp::RSVP_ENGLISH_COMPANION_EXPERIMENTAL.size(), 34u);
  for (const auto& entry : rsvp::RSVP_ENGLISH_COMPANION_CORE)
    EXPECT_EQ(rsvp::classifyRsvpCompanion(entry.text, rsvp::GroupingLanguage::English), CompanionRole::Forward);
  for (const auto& entry : rsvp::RSVP_ENGLISH_COMPANION_EXPERIMENTAL)
    EXPECT_EQ(rsvp::classifyRsvpCompanion(entry.text, rsvp::GroupingLanguage::English), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("don't", rsvp::GroupingLanguage::English), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("'don't'", rsvp::GroupingLanguage::English), CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("wouldn\xE2\x80\x99t", rsvp::GroupingLanguage::English),
            CompanionRole::Forward);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("'house'", rsvp::GroupingLanguage::English), CompanionRole::None);
  EXPECT_EQ(rsvp::classifyRsvpCompanion("dogs'", rsvp::GroupingLanguage::English), CompanionRole::None);
}

TEST(RsvpCompanionPolicy, AppliesLetterLimitsAndMetadataChoice) {
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("из-за"), 4u);
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("e\xCC\x81"), 1u);
  EXPECT_TRUE(rsvp::isRsvpCompanionEligible("without", rsvp::GroupingLanguage::English, 7));
  EXPECT_FALSE(rsvp::isRsvpCompanionEligible("without", rsvp::GroupingLanguage::English, 5));
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, " en-US "),
            rsvp::GroupingLanguage::English);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "ru-RU"),
            rsvp::GroupingLanguage::Russian);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "en_US"), rsvp::GroupingLanguage::None);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "eu"), rsvp::GroupingLanguage::None);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "en-x"), rsvp::GroupingLanguage::None);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "en-US-US"),
            rsvp::GroupingLanguage::None);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::Auto, "en-Latn-US"),
            rsvp::GroupingLanguage::English);
  EXPECT_EQ(rsvp::resolveGroupingLanguage(rsvp::GroupingLanguageChoice::English, "xx"),
            rsvp::GroupingLanguage::English);
}

TEST(RsvpCompanionPolicy, EnglishInventoryHasExactlyTheAgreedForms) {
  static constexpr const char* EXCLUDED[] = {"is",   "are",  "have", "can",     "his",    "her",   "me",   "them",
                                             "this", "that", "if",   "because", "before", "after", "only", "never"};
  static constexpr const char* EXPECTED[] = {
      "a",      "an",     "the",     "my",      "your",    "our",     "its",     "their",  "of",     "to",
      "from",   "with",   "among",   "during",  "at",      "in",      "on",      "by",     "for",    "into",
      "onto",   "upon",   "under",   "over",    "about",   "around",  "behind",  "beside", "beyond", "near",
      "toward", "within", "without", "against", "between", "towards", "through", "I",      "you",    "he",
      "she",    "it",     "we",      "they",    "and",     "or",      "but",     "not"};
  for (const char* word : EXPECTED)
    EXPECT_EQ(rsvp::classifyRsvpCompanion(word, rsvp::GroupingLanguage::English), CompanionRole::Forward) << word;
  EXPECT_EQ(rsvp::classifyRsvpCompanion("(WITH)", rsvp::GroupingLanguage::English), CompanionRole::Forward);
  for (const char* word : EXCLUDED)
    EXPECT_EQ(rsvp::classifyRsvpCompanion(word, rsvp::GroupingLanguage::English), CompanionRole::None) << word;
}

TEST(RsvpCompanionPolicy, AcceptsValidBcp47VariantsAndRejectsMalformedTags) {
  using rsvp::GroupingLanguage;
  using rsvp::GroupingLanguageChoice;
  const char* valid[] = {"en",    "EN",         "en-US",           "en-GB",  "en-Latn-US",  "ru",
                         "ru-RU", "ru-Cyrl-RU", "en-u-ca-gregory", "en-x-a", "en-x-reader", "ru-Cyrl-RU-x-book"};
  for (const char* tag : valid)
    EXPECT_NE(rsvp::resolveGroupingLanguage(GroupingLanguageChoice::Auto, tag), GroupingLanguage::None) << tag;
  const char* malformed[] = {"en_",      "en_US", "en--US",       "en-",  "en-x",         "en-a",
                             "en-US-US", "en-12", "en-1234-1234", "en-u", "en-u-ca-u-nu", " en US "};
  for (const char* tag : malformed)
    EXPECT_EQ(rsvp::resolveGroupingLanguage(GroupingLanguageChoice::Auto, tag), GroupingLanguage::None) << tag;
}

TEST(RsvpCompanionPolicy, AccentsApostrophesAndLetterLimitsAreUnicodeSafe) {
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("\xC3\xA9't"), 2u);   // é't
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("e\xCC\x81't"), 2u);  // é't
  EXPECT_EQ(rsvp::classifyRsvpCompanion("e\xCC\x81't", rsvp::GroupingLanguage::English), CompanionRole::Forward);
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("123-with"), 4u);
  EXPECT_TRUE(rsvp::isRsvpCompanionEligible("e\xCC\x81't", rsvp::GroupingLanguage::English, 2));
  EXPECT_TRUE(rsvp::isRsvpCompanionEligible("without", rsvp::GroupingLanguage::English, 7));
  EXPECT_FALSE(rsvp::isRsvpCompanionEligible("shouldn't", rsvp::GroupingLanguage::English, 7));
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount("'dogs'"), 4u);

  const std::string saturated(300, 'a');
  EXPECT_EQ(rsvp::rsvpCompanionLetterCount(saturated), 255u);
}

}  // namespace
