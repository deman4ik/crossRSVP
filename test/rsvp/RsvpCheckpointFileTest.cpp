#include <gtest/gtest.h>

#include <HalStorage.h>

#include <filesystem>
#include <fstream>

#include "RsvpCheckpointFile.h"

namespace {

namespace fs = std::filesystem;

class RsvpCheckpointFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory = fs::temp_directory_path() / "crossrsvp_checkpoint_file" /
                ::testing::UnitTest::GetInstance()->current_test_info()->name();
    fs::remove_all(directory);
    fs::create_directories(directory);
    Storage.resetTestState();
  }

  void TearDown() override {
    Storage.resetTestState();
    fs::remove_all(directory);
  }

  rsvp::RsvpCheckpoint checkpoint(const uint32_t offset) const {
    return {.bookRevision = 0x1122334455667788ULL,
            .anchor = {.spineIndex = 2, .visibleTextOffset = offset, .sameOffsetOrdinal = 1, .valid = true},
            .tokenHash32 = 0xCAFEBABEU,
            .tokenLength = 12,
            .activeRsvpTimeMs = 5000};
  }

  std::string path() const { return (directory / "rsvp_checkpoint.bin").string(); }
  std::string temporary() const { return path() + ".tmp"; }
  std::string backup() const { return path() + ".bak"; }

  fs::path directory;
};

TEST_F(RsvpCheckpointFileTest, FailedPromotionPreservesTheLastKnownGoodCheckpoint) {
  const auto original = checkpoint(10);
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), original));

  Storage.failRename(temporary(), path());
  EXPECT_FALSE(rsvp::RsvpCheckpointFile::save(directory.string(), checkpoint(20)));

  rsvp::RsvpCheckpoint loaded;
  ASSERT_EQ(rsvp::RsvpCheckpointFile::load(directory.string(), original.bookRevision, loaded),
            rsvp::CheckpointStatus::Ok);
  EXPECT_EQ(loaded.anchor.visibleTextOffset, 10U);
}

TEST_F(RsvpCheckpointFileTest, BackupRecoversAfterPromotionAndImmediateRestoreBothFail) {
  const auto original = checkpoint(10);
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), original));

  Storage.failRename(temporary(), path());
  Storage.failRename(backup(), path());
  EXPECT_FALSE(rsvp::RsvpCheckpointFile::save(directory.string(), checkpoint(20)));
  EXPECT_FALSE(Storage.exists(path().c_str()));
  EXPECT_TRUE(Storage.exists(backup().c_str()));

  rsvp::RsvpCheckpoint loaded;
  ASSERT_EQ(rsvp::RsvpCheckpointFile::load(directory.string(), original.bookRevision, loaded),
            rsvp::CheckpointStatus::Ok);
  EXPECT_EQ(loaded.anchor.visibleTextOffset, 10U);
}

TEST_F(RsvpCheckpointFileTest, FailedBackupPromotionReportsTheInvalidBackupStatus) {
  const auto value = checkpoint(10);
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), value));
  ASSERT_EQ(std::rename(path().c_str(), backup().c_str()), 0);
  {
    std::ofstream output(backup(), std::ios::binary | std::ios::trunc);
  }
  Storage.failRename(backup(), path());

  rsvp::RsvpCheckpoint loaded;
  EXPECT_EQ(rsvp::RsvpCheckpointFile::load(directory.string(), value.bookRevision, loaded),
            rsvp::CheckpointStatus::Truncated);
}

TEST_F(RsvpCheckpointFileTest, IdenticalCheckpointSuppressesFilesystemPromotion) {
  const auto value = checkpoint(10);
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), value));
  Storage.resetTestState();

  EXPECT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), value));
  EXPECT_EQ(Storage.renameCount(), 0U);
}

TEST_F(RsvpCheckpointFileTest, ExistingButUnreadableCheckpointIsNotReportedAsMissing) {
  const auto value = checkpoint(10);
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), value));
  Storage.failNextReadOpen(path());

  rsvp::RsvpCheckpoint loaded;
  EXPECT_EQ(rsvp::RsvpCheckpointFile::load(directory.string(), value.bookRevision, loaded),
            rsvp::CheckpointStatus::ReadError);
}

TEST_F(RsvpCheckpointFileTest, BookRevisionChangesWhenBookBytesChange) {
  const fs::path book = directory / "book.epub";
  {
    std::ofstream output(book, std::ios::binary);
    output << "first revision";
  }
  uint64_t first = 0;
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::computeBookRevision(book.string(), first));

  {
    std::ofstream output(book, std::ios::binary | std::ios::trunc);
    output << "second revision";
  }
  uint64_t second = 0;
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::computeBookRevision(book.string(), second));
  EXPECT_NE(first, second);
}

}  // namespace
