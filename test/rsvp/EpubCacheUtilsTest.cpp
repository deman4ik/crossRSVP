#include <HalStorage.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "EpubCacheUtils.h"

namespace {

namespace fs = std::filesystem;

class EpubCacheUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    cachePath = fs::temp_directory_path() / "crossrsvp_epub_cache" /
                ::testing::UnitTest::GetInstance()->current_test_info()->name();
    fs::remove_all(cachePath);
    fs::create_directories(cachePath / "sections");
    Storage.resetTestState();
  }

  void TearDown() override {
    Storage.resetTestState();
    fs::remove_all(cachePath);
  }

  void writeEntry(const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output);
    output << "cache";
  }

  fs::path cachePath;
};

TEST_F(EpubCacheUtilsTest, DerivedClearPreservesDurableReaderState) {
  writeEntry(cachePath / "progress.bin");
  writeEntry(cachePath / "rsvp_checkpoint.bin");
  writeEntry(cachePath / "rsvp_checkpoint.bin.bak");
  writeEntry(cachePath / "grouping-language.bin");
  writeEntry(cachePath / "grouping-language.bin.bak");
  writeEntry(cachePath / "cover.bmp");
  writeEntry(cachePath / "book.bin");
  writeEntry(cachePath / "sections" / "section_0.bin");

  ASSERT_TRUE(clearEpubDerivedCache(cachePath.string()));

  EXPECT_TRUE(fs::exists(cachePath / "progress.bin"));
  EXPECT_TRUE(fs::exists(cachePath / "rsvp_checkpoint.bin"));
  EXPECT_TRUE(fs::exists(cachePath / "rsvp_checkpoint.bin.bak"));
  EXPECT_TRUE(fs::exists(cachePath / "grouping-language.bin"));
  EXPECT_TRUE(fs::exists(cachePath / "grouping-language.bin.bak"));
  EXPECT_FALSE(fs::exists(cachePath / "cover.bmp"));
  EXPECT_FALSE(fs::exists(cachePath / "book.bin"));
  EXPECT_FALSE(fs::exists(cachePath / "sections"));
}

TEST_F(EpubCacheUtilsTest, DestructiveRemovalRemovesDurableReaderState) {
  writeEntry(cachePath / "progress.bin");
  writeEntry(cachePath / "rsvp_checkpoint.bin");
  writeEntry(cachePath / "rsvp_checkpoint.bin.bak");

  ASSERT_TRUE(clearEpubDerivedCache(cachePath.string()));
  ASSERT_TRUE(Storage.exists((cachePath / "progress.bin").c_str()));

  ASSERT_TRUE(Storage.removeDir(cachePath.c_str()));
  EXPECT_FALSE(fs::exists(cachePath));
}

TEST_F(EpubCacheUtilsTest, MissingCacheIsAlreadyClear) {
  fs::remove_all(cachePath);

  EXPECT_TRUE(clearEpubDerivedCache(cachePath.string()));
}

}  // namespace
