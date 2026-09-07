#include <gtest/gtest.h>

#include <cstring>
#include <utility>
#include <vector>

#include "RsvpSession.h"

namespace {

rsvp::DocumentEvent word(const char* text, uint32_t offset) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::Word;
  event.anchor = {.visibleTextOffset = offset, .valid = true};
  event.textLength = static_cast<uint16_t>(strlen(text));
  memcpy(event.text, text, event.textLength + 1);
  return event;
}

rsvp::DocumentEvent end(uint32_t offset) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::EndOfBook;
  event.anchor = {.visibleTextOffset = offset, .valid = true};
  return event;
}

class Source final : public rsvp::RsvpSource {
 public:
  explicit Source(std::vector<rsvp::DocumentEvent> events) : events_(std::move(events)) {}
  bool open(const rsvp::ResumeAnchor*) override {
    index_ = 0;
    return true;
  }
  bool next(rsvp::DocumentEvent& event) override {
    if (index_ >= events_.size()) return false;
    event = events_[index_++];
    return true;
  }

 private:
  std::vector<rsvp::DocumentEvent> events_;
  size_t index_ = 0;
};

rsvp::PresentationGroupRange acceptAll(void*, const rsvp::PreparedWord&, const rsvp::PresentationGroup& group) {
  return {0, group.count};
}

TEST(RsvpSessionConfiguration, PausedPaceIsClampedAndCurrentFrameIsPreserved) {
  Source source({word("one", 0), word("two", 4), end(8)});
  rsvp::RsvpSession session(source);
  const auto first = session.step({});
  ASSERT_EQ(first.state, rsvp::State::Paused);
  ASSERT_TRUE(first.frame.presentationGroup != nullptr);
  const auto anchor = session.currentAnchor();
  const auto configured =
      session.configureWhilePaused({.paceWpm = 999, .minimumWpm = 80, .maximumWpm = 200, .safeMaximumWpm = 150}, false);
  EXPECT_TRUE(configured.render);
  EXPECT_EQ(configured.state, rsvp::State::Paused);
  EXPECT_EQ(configured.paceWpm, 150);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, anchor.visibleTextOffset);
  ASSERT_NE(configured.frame.presentationGroup, nullptr);
  EXPECT_STREQ(configured.frame.presentationGroup->tokens[configured.frame.presentationGroup->activeIndex].text, "one");
}

TEST(RsvpSessionConfiguration, NewGroupingAppliesOnlyToFutureGroup) {
  Source source({word("я", 0), word("не", 3), word("сделал", 6), word("бы", 13), end(16)});
  rsvp::RsvpSession session(source, {}, {}, false, acceptAll, nullptr);
  const auto first = session.step({});
  ASSERT_EQ(first.frame.presentationGroup->count, 1);
  const auto configured = session.configureWhilePaused({}, true);
  ASSERT_TRUE(configured.render);
  ASSERT_EQ(configured.frame.presentationGroup->count, 1);
  session.step({.action = rsvp::Action::FramePresented, .presentedFrameId = configured.frame.id});
  const auto next = session.step({.action = rsvp::Action::StepForward});
  ASSERT_NE(next.frame.presentationGroup, nullptr);
  EXPECT_EQ(next.frame.presentationGroup->count, 3);
  EXPECT_STREQ(next.frame.presentationGroup->tokens[0].text, "не");
}

TEST(RsvpSessionConfiguration, PlayingSessionRejectsMutation) {
  Source source({word("one", 0), word("two", 4), end(8)});
  rsvp::RsvpSession session(source);
  const auto first = session.step({});
  session.step({.action = rsvp::Action::FramePresented, .presentedFrameId = first.frame.id});
  const auto playing = session.step({.action = rsvp::Action::TogglePlayback});
  ASSERT_EQ(playing.state, rsvp::State::Playing);
  const auto rejected = session.configureWhilePaused({.paceWpm = 200}, true);
  EXPECT_FALSE(rejected.render);
  EXPECT_EQ(rejected.state, rsvp::State::Playing);
  EXPECT_EQ(rejected.paceWpm, playing.paceWpm);
}

}  // namespace
