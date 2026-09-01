#include "TextBlock.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

TEST(TextBlockTest, VisibleOffsetsSurviveCacheRoundTrip) {
  const std::vector<std::string> words = {"first", "second", "third"};
  const std::vector<int16_t> xPositions = {0, 40, 90};
  const std::vector<EpdFontFamily::Style> styles(words.size(), EpdFontFamily::REGULAR);
  const std::vector<uint32_t> visibleOffsets = {12, 19, 31};

  TextBlock block(words, xPositions, styles, visibleOffsets, {}, {});
  ASSERT_TRUE(block.valid());
  EXPECT_EQ(block.wordVisibleTextOffset(1), 19u);
  ASSERT_TRUE(block.findWordForVisibleTextOffset(31u).has_value());
  EXPECT_EQ(*block.findWordForVisibleTextOffset(31u), 2u);

  HalFile file;
  ASSERT_TRUE(file.open("text-block-cache-test.bin", "w+b"));
  ASSERT_TRUE(block.serialize(file));
  ASSERT_TRUE(file.seek(0));

  const auto restored = TextBlock::deserialize(file);
  ASSERT_NE(restored, nullptr);
  ASSERT_TRUE(restored->valid());
  ASSERT_EQ(restored->wordCount(), words.size());
  EXPECT_EQ(restored->wordVisibleTextOffset(0), 12u);
  EXPECT_EQ(restored->wordVisibleTextOffset(2), 31u);
  ASSERT_TRUE(restored->findWordForVisibleTextOffset(19u).has_value());
  EXPECT_EQ(*restored->findWordForVisibleTextOffset(19u), 1u);

  file.close();
  std::remove("text-block-cache-test.bin");
}
