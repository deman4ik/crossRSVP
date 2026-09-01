#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "RsvpLexicalCore.h"
#include "RsvpWordLayout.h"
#include "Utf8.h"

namespace {

rsvp::PreparedWord prepare(const char* text) {
  rsvp::PreparedWord word;
  EXPECT_TRUE(rsvp::prepareRsvpWord(text, strlen(text), word));
  return word;
}

std::string slice(const rsvp::PreparedWord& word, const rsvp::ByteRange range) {
  return std::string(word.text + range.begin, range.end - range.begin);
}

}  // namespace

TEST(Utf8BoundedCompose, ComposesIntoCallerBufferWithoutAllocation) {
  constexpr char decomposed[] = "e\xCC\x81";
  char output[16] = {};
  size_t written = 0;

  ASSERT_TRUE(utf8ComposeNfcToBuffer(decomposed, sizeof(decomposed) - 1,
                                    output, sizeof(output), written));
  EXPECT_EQ(written, 2u);
  EXPECT_STREQ(output, "\xC3\xA9");
}

TEST(Utf8BoundedCompose, RejectsOutputThatCannotFit) {
  constexpr char text[] = "hello";
  char output[4] = {};
  size_t written = 0;

  EXPECT_FALSE(utf8ComposeNfcToBuffer(text, sizeof(text) - 1, output, sizeof(output), written));
}

TEST(RsvpLexicalCore, UsesUnicodeGraphemesAndKeepsPunctuationVisible) {
  const auto word = prepare("\xC2\xAB\xD0\xB5\xCC\x88-\xD0\xBA\xD0\xBE\xD1\x82\xC2\xBB?!");

  ASSERT_TRUE(word.valid);
  EXPECT_STREQ(word.text, "\xC2\xAB\xD1\x91-\xD0\xBA\xD0\xBE\xD1\x82\xC2\xBB?!");
  EXPECT_EQ(word.lexicalLength, 5u);
  EXPECT_EQ(slice(word, word.prefix), "\xC2\xAB");
  EXPECT_EQ(slice(word, word.core), "\xD1\x91-\xD0\xBA\xD0\xBE\xD1\x82");
  EXPECT_EQ(slice(word, word.pivot), "\xD0\xBA");
  EXPECT_EQ(slice(word, word.suffix), "\xC2\xBB?!");
  EXPECT_EQ(word.pauseClass, rsvp::PauseClass::Sentence);
}

TEST(RsvpLexicalCore, AbbreviationPeriodsDoNotBecomeSentencePauses) {
  const auto word = prepare("\xD1\x82.\xD0\xB5.");

  ASSERT_TRUE(word.valid);
  EXPECT_EQ(word.lexicalLength, 2u);
  EXPECT_EQ(slice(word, word.core), "\xD1\x82.\xD0\xB5");
  EXPECT_EQ(slice(word, word.pivot), "\xD0\xB5");
  EXPECT_EQ(slice(word, word.suffix), ".");
  EXPECT_EQ(word.pauseClass, rsvp::PauseClass::None);
}

TEST(RsvpLexicalCore, ClosingQuotesAndBracketsDoNotHidePausePunctuation) {
  const auto clause = prepare("word,\xC2\xBB]");
  const auto sentence = prepare("word?!\xC2\xBB]");

  EXPECT_EQ(clause.pauseClass, rsvp::PauseClass::Clause);
  EXPECT_EQ(sentence.pauseClass, rsvp::PauseClass::Sentence);
}

TEST(RsvpLexicalCore, NumbersAndInternalHyphensAreLexical) {
  const auto word = prepare("12-34");

  ASSERT_TRUE(word.valid);
  EXPECT_EQ(word.lexicalLength, 5u);
  EXPECT_EQ(slice(word, word.pivot), "-");
}

TEST(RsvpLexicalCore, SoftHyphenIsRemovedBeforeRangesAreComputed) {
  const auto word = prepare("co\xC2\xADoperate");

  ASSERT_TRUE(word.valid);
  EXPECT_STREQ(word.text, "cooperate");
  EXPECT_EQ(word.textLength, 9u);
  EXPECT_EQ(word.lexicalLength, 9u);
}

TEST(RsvpLexicalCore, ReportsLongWordsWithoutPartialPreparedText) {
  char longWord[rsvp::MAX_TOKEN_BYTES + 2] = {};
  for (size_t index = 0; index < rsvp::MAX_TOKEN_BYTES + 1; ++index) {
    longWord[index] = 'a';
  }

  rsvp::PreparedWord word;
  EXPECT_FALSE(rsvp::prepareRsvpWord(longWord, rsvp::MAX_TOKEN_BYTES + 1,
                                     word));
  EXPECT_TRUE(word.overflowed);
  EXPECT_EQ(word.textLength, 0u);
}

TEST(RsvpWordLayout, KeepsIntegerPivotAtFocusAndChecksBounds) {
  rsvp::RsvpWordLayout layout;
  ASSERT_TRUE(rsvp::calculateRsvpWordLayout(100, 0, 240, 31, 11, 22, layout));

  EXPECT_EQ(layout.pivotX + (layout.pivotAdvance + 1) / 2, 100);
  EXPECT_EQ(layout.startX, 64);
  EXPECT_EQ(layout.totalWidth, 64);
  EXPECT_TRUE(layout.fits);
}

TEST(RsvpWordLayout, ReportsOverflowWithoutMovingTheFocus) {
  rsvp::RsvpWordLayout layout;
  EXPECT_FALSE(rsvp::calculateRsvpWordLayout(20, 0, 40, 25, 10, 25, layout));
  EXPECT_EQ(layout.pivotX + (layout.pivotAdvance + 1) / 2, 20);
  EXPECT_FALSE(layout.fits);
}
