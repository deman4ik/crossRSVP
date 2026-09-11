#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "ContentOpfParser.h"

namespace {
std::string parseLanguage(const std::string& metadata) {
  const std::string xml = "<package xmlns:dc=\"urn:uuid:test\"><metadata>" + metadata + "</metadata></package>";
  const std::string cachePath = (std::filesystem::temp_directory_path() / "crosspoint-epub-metadata" /
                                 ::testing::UnitTest::GetInstance()->current_test_info()->name())
                                    .string();
  const std::string baseContentPath;
  ContentOpfParser parser(cachePath, baseContentPath, xml.size(), nullptr);
  EXPECT_TRUE(parser.setup());
  const size_t split = xml.size() / 2;
  EXPECT_EQ(parser.write(reinterpret_cast<const uint8_t*>(xml.data()), split), split);
  EXPECT_EQ(parser.write(reinterpret_cast<const uint8_t*>(xml.data() + split), xml.size() - split), xml.size() - split);
  return parser.language;
}
}  // namespace

TEST(EpubMetadata, FirstLanguageElementWins) {
  EXPECT_EQ(parseLanguage("<language> en-US </language><language>ru-RU</language>"), "en-US");
  EXPECT_EQ(parseLanguage("<language></language><language>en</language>"), "");
}

TEST(EpubMetadata, LanguageCharacterChunksAreJoinedAndTrimmed) {
  EXPECT_EQ(parseLanguage("<language>\n  ru-RU\n</language>"), "ru-RU");
}

TEST(EpubMetadata, UnsupportedAndMalformedMetadataRemainPredictable) {
  EXPECT_EQ(parseLanguage("<language>fr</language><language>en</language>"), "fr");
  const std::string malformed = "<package><metadata><language>en";
  const std::string cachePath = (std::filesystem::temp_directory_path() / "crosspoint-epub-metadata" /
                                 ::testing::UnitTest::GetInstance()->current_test_info()->name())
                                    .string();
  const std::string baseContentPath;
  ContentOpfParser parser(cachePath, baseContentPath, malformed.size(), nullptr);
  ASSERT_TRUE(parser.setup());
  EXPECT_EQ(parser.write(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size()), 0u);
}
