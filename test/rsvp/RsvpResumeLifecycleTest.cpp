#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

#include "RsvpCheckpointFile.h"
#include "RsvpModeSwitch.h"
#include "RsvpSession.h"

namespace {

namespace fs = std::filesystem;

rsvp::DocumentEvent lifecycleWord(const char* text, const uint32_t offset) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::Word;
  event.anchor = {.spineIndex = 0, .visibleTextOffset = offset, .sameOffsetOrdinal = 0, .valid = true};
  event.textLength = static_cast<uint16_t>(std::strlen(text));
  std::memcpy(event.text, text, event.textLength + 1);
  return event;
}

class LifecycleSource final : public rsvp::RsvpSource {
 public:
  explicit LifecycleSource(std::vector<rsvp::DocumentEvent> events) : events(std::move(events)) {}

  bool open(const rsvp::ResumeAnchor* anchor) override {
    index = 0;
    if (!anchor || !anchor->valid) return true;
    while (index < events.size() && events[index].anchor.visibleTextOffset < anchor->visibleTextOffset) ++index;
    return true;
  }

  bool next(rsvp::DocumentEvent& event) override {
    if (index >= events.size()) return false;
    event = events[index++];
    return true;
  }

 private:
  std::vector<rsvp::DocumentEvent> events;
  size_t index = 0;
};

TEST(RsvpResumeLifecycle, ReopenPagedThenRsvpRestoresThePersistedPresentedWordPaused) {
  const auto directory = fs::temp_directory_path() / "crossrsvp_resume_lifecycle";
  fs::remove_all(directory);
  fs::create_directories(directory);
  Storage.resetTestState();

  LifecycleSource originalSource(
      {lifecycleWord("первое", 0), lifecycleWord("последнее", 20), {.kind = rsvp::EventKind::EndOfBook}});
  rsvp::RsvpSession original(originalSource, {.spineIndex = 0, .visibleTextOffset = 20, .valid = true});
  const auto presented = original.step({.nowMs = 10});
  ASSERT_TRUE(presented.render);
  ASSERT_TRUE(original
                  .step({.nowMs = 20,
                         .action = rsvp::Action::FramePresented,
                         .presentedFrameId = presented.frame.id,
                         .refreshDurationMs = 10})
                  .presentationAccepted);

  constexpr uint64_t revision = 0xA1B2C3D4ULL;
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(), {.bookRevision = revision,
                                                                  .anchor = original.currentAnchor(),
                                                                  .tokenHash32 = original.currentTokenHash(),
                                                                  .tokenLength = original.currentTokenLength()}));

  const auto route =
      rsvp::RsvpModeSwitch::fromPaged({.currentAnchor = {},
                                       .pageStartAnchor = {.spineIndex = 0, .visibleTextOffset = 0, .valid = true},
                                       .explicitNavigation = false});
  ASSERT_TRUE(route.restoreCheckpoint);

  rsvp::RsvpCheckpoint restored;
  ASSERT_EQ(rsvp::RsvpCheckpointFile::load(directory.string(), revision, restored), rsvp::CheckpointStatus::Ok);
  LifecycleSource reopenedSource(
      {lifecycleWord("первое", 0), lifecycleWord("последнее", 20), {.kind = rsvp::EventKind::EndOfBook}});
  rsvp::RsvpSession reopened(reopenedSource, restored.anchor);
  const auto resumed = reopened.step({.nowMs = 30});

  ASSERT_TRUE(resumed.render);
  EXPECT_EQ(resumed.state, rsvp::State::Paused);
  EXPECT_EQ(std::string(resumed.frame.text, resumed.frame.textLength), "последнее");
  EXPECT_EQ(reopened.requestedTokenHash(), restored.tokenHash32);
  EXPECT_EQ(reopened.requestedTokenLength(), restored.tokenLength);

  Storage.resetTestState();
  fs::remove_all(directory);
}

TEST(RsvpResumeLifecycle, FailedRejectedCheckpointRemovalDoesNotBlockRsvpReentry) {
  const auto directory = fs::temp_directory_path() / "crossrsvp_resume_rejected_checkpoint";
  fs::remove_all(directory);
  fs::create_directories(directory);
  Storage.resetTestState();

  constexpr uint64_t revision = 0xA1B2C3D4ULL;
  const auto rejectedPath = (directory / "rsvp_checkpoint.bin").string();
  ASSERT_TRUE(rsvp::RsvpCheckpointFile::save(directory.string(),
                                             {.bookRevision = revision,
                                              .anchor = {.spineIndex = 0, .visibleTextOffset = 20, .valid = true},
                                              .tokenHash32 = 1,
                                              .tokenLength = 1}));
  Storage.failRemove(rejectedPath);
  ASSERT_FALSE(rsvp::RsvpCheckpointFile::invalidate(directory.string()));

  const auto route =
      rsvp::RsvpModeSwitch::fromPaged({.currentAnchor = {},
                                       .pageStartAnchor = {.spineIndex = 0, .visibleTextOffset = 0, .valid = true},
                                       .checkpointRestoreSuppressed = true});
  ASSERT_FALSE(route.restoreCheckpoint);
  LifecycleSource source({lifecycleWord("первое", 0), lifecycleWord("последнее", 20)});
  rsvp::RsvpSession session(source, route.anchor);
  const auto resumed = session.step({.nowMs = 10});

  ASSERT_TRUE(resumed.render);
  EXPECT_EQ(resumed.state, rsvp::State::Paused);
  EXPECT_EQ(std::string(resumed.frame.text, resumed.frame.textLength), "первое");
  EXPECT_TRUE(rsvp::RsvpCheckpointFile::invalidate(directory.string()));
  EXPECT_FALSE(Storage.exists(rejectedPath.c_str()));

  Storage.resetTestState();
  fs::remove_all(directory);
}

}  // namespace
