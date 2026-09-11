#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Epub/BookGroupingLanguagePreference.h"
#include "HalStorage.h"

namespace {
class BookGroupingLanguagePreferenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::temp_directory_path() / "crosspoint-grouping-language-test" /
           ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Storage.resetTestState();
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  std::filesystem::path root;
};
}  // namespace

void writeBytes(const std::filesystem::path& path, const std::initializer_list<uint8_t> bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file);
  for (const uint8_t byte : bytes) file.put(static_cast<char>(byte));
  ASSERT_TRUE(file.good());
}

void writeBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file);
  for (const uint8_t byte : bytes) file.put(static_cast<char>(byte));
  ASSERT_TRUE(file.good());
}

TEST_F(BookGroupingLanguagePreferenceTest, MissingDefaultsToAutoAndRoundTrips) {
  BookGroupingLanguagePreference preference(root.string());
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::Auto);
  ASSERT_TRUE(preference.save(BookGroupingLanguagePreference::Choice::English));
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::English);
  ASSERT_TRUE(preference.save(BookGroupingLanguagePreference::Choice::Russian));
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::Russian);
}

TEST_F(BookGroupingLanguagePreferenceTest, IndependentBookPathsAndRedundantSave) {
  BookGroupingLanguagePreference first((root / "one").string());
  BookGroupingLanguagePreference second((root / "two").string());
  ASSERT_TRUE(first.save(BookGroupingLanguagePreference::Choice::Russian));
  ASSERT_TRUE(second.save(BookGroupingLanguagePreference::Choice::English));
  EXPECT_EQ(first.load(), BookGroupingLanguagePreference::Choice::Russian);
  EXPECT_EQ(second.load(), BookGroupingLanguagePreference::Choice::English);
  const auto renames = Storage.renameCount();
  ASSERT_TRUE(first.save(BookGroupingLanguagePreference::Choice::Russian));
  EXPECT_EQ(Storage.renameCount(), renames);
}

TEST_F(BookGroupingLanguagePreferenceTest, InvalidOrTruncatedFileDefaultsToAuto) {
  const auto path = root / "grouping-language.bin";
  std::filesystem::create_directories(root);
  {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    const uint8_t bad[] = {'C', 'P', 'G', 'L', 1};
    ASSERT_EQ(std::fwrite(bad, 1, sizeof(bad), file), sizeof(bad));
    std::fclose(file);
  }
  BookGroupingLanguagePreference preference(root.string());
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::Auto);
}

TEST_F(BookGroupingLanguagePreferenceTest, CanReselectAutoAndPersistIt) {
  BookGroupingLanguagePreference preference(root.string());
  ASSERT_TRUE(preference.save(BookGroupingLanguagePreference::Choice::English));
  ASSERT_TRUE(preference.save(BookGroupingLanguagePreference::Choice::Auto));
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::Auto);
  EXPECT_TRUE(std::filesystem::exists(root / "grouping-language.bin"));
}

TEST_F(BookGroupingLanguagePreferenceTest, RecoversValidBackupAfterInterruptedPromotion) {
  writeBytes(root / "grouping-language.bin.bak", {'C', 'P', 'G', 'L', 1, 1});
  BookGroupingLanguagePreference preference(root.string());
  EXPECT_EQ(preference.load(), BookGroupingLanguagePreference::Choice::Russian);
  EXPECT_TRUE(std::filesystem::exists(root / "grouping-language.bin"));
}

TEST_F(BookGroupingLanguagePreferenceTest, RejectsInvalidMagicVersionValueAndLength) {
  const std::vector<std::vector<uint8_t>> invalid = {
      {'X', 'P', 'G', 'L', 1, 1},
      {'C', 'P', 'G', 'L', 2, 1},
      {'C', 'P', 'G', 'L', 1, 3},
      {'C', 'P', 'G', 'L', 1, 1, 0},
  };
  for (const auto& bytes : invalid) {
    writeBytes(root / "grouping-language.bin", bytes);
    EXPECT_EQ(BookGroupingLanguagePreference(root.string()).load(), BookGroupingLanguagePreference::Choice::Auto);
  }
}
