#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "RsvpSession.h"

namespace {

rsvp::DocumentEvent word(const char* text, const uint32_t offset) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::Word;
  event.anchor = {.visibleTextOffset = offset, .valid = true};
  event.textLength = static_cast<uint16_t>(std::strlen(text));
  std::memcpy(event.text, text, event.textLength + 1);
  return event;
}

rsvp::DocumentEvent marker(const rsvp::EventKind kind, const uint32_t offset) {
  rsvp::DocumentEvent event;
  event.kind = kind;
  event.anchor = {.visibleTextOffset = offset, .valid = true};
  return event;
}

rsvp::DocumentEvent punctuation(const char* text, const uint32_t offset) {
  auto event = marker(rsvp::EventKind::NonLexicalText, offset);
  event.textLength = static_cast<uint16_t>(std::strlen(text));
  std::memcpy(event.text, text, event.textLength + 1);
  return event;
}

uint32_t hash(const char* text, const uint16_t length) {
  uint32_t result = 2166136261U;
  for (uint16_t index = 0; index < length; ++index) {
    result ^= static_cast<uint8_t>(text[index]);
    result *= 16777619U;
  }
  return result;
}

class Source final : public rsvp::RsvpSource {
 public:
  explicit Source(std::vector<rsvp::DocumentEvent> events) : events(std::move(events)) {}

  bool open(const rsvp::ResumeAnchor* anchor) override {
    ++openCount;
    index = 0;
    if (anchor == nullptr || !anchor->valid) return true;
    for (size_t candidate = 0; candidate < events.size(); ++candidate) {
      const auto& event = events[candidate];
      if (!event.anchor.valid || event.anchor.visibleTextOffset < anchor->visibleTextOffset) continue;
      if (event.anchor.visibleTextOffset == anchor->visibleTextOffset &&
          event.anchor.sameOffsetOrdinal < anchor->sameOffsetOrdinal)
        continue;
      index = candidate;
      return true;
    }
    index = events.size();
    return true;
  }

  bool next(rsvp::DocumentEvent& out) override {
    if (index >= events.size()) return false;
    out = events[index++];
    return true;
  }

  std::vector<rsvp::DocumentEvent> events;
  size_t index = 0;
  size_t openCount = 0;
};

rsvp::PresentationGroupRange acceptAll(void*, const rsvp::PreparedWord&, const rsvp::PresentationGroup& group) {
  return {0, group.count};
}

rsvp::PresentationGroupRange activeOnly(void*, const rsvp::PreparedWord&, const rsvp::PresentationGroup& group) {
  return {group.activeIndex, static_cast<uint8_t>(group.activeIndex + 1)};
}

rsvp::PresentationGroupRange changedFit(void* context, const rsvp::PreparedWord&,
                                        const rsvp::PresentationGroup& group) {
  return context != nullptr && *static_cast<bool*>(context)
             ? rsvp::PresentationGroupRange{group.activeIndex, static_cast<uint8_t>(group.activeIndex + 1)}
             : rsvp::PresentationGroupRange{0, group.count};
}

std::vector<std::string> groupText(const rsvp::Decision& decision) {
  EXPECT_NE(decision.frame.presentationGroup, nullptr);
  if (decision.frame.presentationGroup == nullptr) return {};
  const auto& group = *decision.frame.presentationGroup;
  std::vector<std::string> result;
  result.reserve(group.count);
  for (uint8_t index = 0; index < group.count; ++index) {
    result.emplace_back(group.tokens[index].text, group.tokens[index].textLength);
  }
  return result;
}

rsvp::Decision acknowledge(rsvp::RsvpSession& session, const rsvp::Decision& frame, const uint32_t nowMs = 0) {
  return session.step({.nowMs = nowMs, .action = rsvp::Action::FramePresented, .presentedFrameId = frame.frame.id});
}

TEST(RsvpPresentationGroup, PrefixAndPostfixWordsStayInSourceOrder) {
  Source source(
      {word("я", 0), word("не", 3), word("сделал", 6), word("бы", 13), marker(rsvp::EventKind::EndOfBook, 16)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);

  const auto first = session.step({});
  EXPECT_EQ(groupText(first), (std::vector<std::string>{"я"}));
  acknowledge(session, first);
  const auto second = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(second), (std::vector<std::string>{"не", "сделал", "бы"}));
}

TEST(RsvpPresentationGroup, RejectingCompanionDoesNotConsumeOrReorderIt) {
  Source source({word("не", 0), word("сделал", 3), word("бы", 10), marker(rsvp::EventKind::EndOfBook, 13)});
  rsvp::RsvpSession session(source, {}, {}, true, activeOnly, nullptr);

  const auto first = session.step({});
  EXPECT_EQ(groupText(first), (std::vector<std::string>{"не"}));
  acknowledge(session, first);
  const auto second = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(second), (std::vector<std::string>{"сделал"}));
  acknowledge(session, second);
  const auto third = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(third), (std::vector<std::string>{"бы"}));
}

TEST(RsvpPresentationGroup, BoundariesPreventGroupingAcrossPunctuationAndStructuralEvents) {
  for (const auto boundary : {rsvp::EventKind::ParagraphBoundary, rsvp::EventKind::ChapterBoundary,
                              rsvp::EventKind::NonText, rsvp::EventKind::EndOfBook}) {
    Source source({word("не", 0), marker(boundary, 3), word("сделал", 4), marker(rsvp::EventKind::EndOfBook, 10)});
    rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
    const auto first = session.step({});
    EXPECT_EQ(groupText(first), (std::vector<std::string>{"не"})) << static_cast<int>(boundary);
  }

  for (const char* separator : {",", ";", ":", "-", "—"}) {
    Source source(
        {word("не", 0), punctuation(separator, 3), word("сделал", 4), marker(rsvp::EventKind::EndOfBook, 12)});
    rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
    const auto first = session.step({});
    EXPECT_EQ(groupText(first), (std::vector<std::string>{"не"})) << separator;
  }
}

TEST(RsvpPresentationGroup, AcknowledgementUsesLastWordIdentity) {
  Source source({word("сделал", 20), word("бы", 27), marker(rsvp::EventKind::EndOfBook, 30)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_EQ(groupText(frame), (std::vector<std::string>{"сделал", "бы"}));
  const auto accepted = acknowledge(session, frame);
  ASSERT_TRUE(accepted.presentationAccepted);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 27u);
  EXPECT_EQ(session.currentTokenLength(), static_cast<uint16_t>(std::strlen("бы")));
  EXPECT_EQ(session.currentTokenHash(), hash("бы", static_cast<uint16_t>(std::strlen("бы"))));
}

TEST(RsvpPresentationGroup, GroupDurationAddsThirtyFivePercentPerCompanion) {
  Source source({word("сделал", 0), word("бы", 7), marker(rsvp::EventKind::EndOfBook, 10)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_TRUE(frame.render);
  acknowledge(session, frame, 0);
  const auto playing = session.step({.nowMs = 0, .action = rsvp::Action::TogglePlayback});
  EXPECT_EQ(playing.nextDeadlineMs, 810u);
}

TEST(RsvpPresentationGroup, RewindRestoresWholeGroupHistory) {
  std::vector<rsvp::DocumentEvent> events;
  for (uint32_t index = 0; index < 8; ++index) {
    const uint32_t offset = index * 10;
    events.push_back(word("сделал", offset));
    events.push_back(word("бы", offset + 7));
  }
  events.push_back(marker(rsvp::EventKind::EndOfBook, 100));
  bool changed = false;
  Source source(std::move(events));
  rsvp::RsvpSession session(source, {}, {}, true, changedFit, &changed);
  std::vector<std::vector<std::string>> groups;
  for (int index = 0; index < 8; ++index) {
    const auto frame = session.step(index == 0 ? rsvp::Input{} : rsvp::Input{.action = rsvp::Action::StepForward});
    ASSERT_TRUE(frame.render);
    groups.push_back(groupText(frame));
    acknowledge(session, frame);
  }
  changed = true;
  const auto rewind = session.step({.action = rsvp::Action::RewindFive});
  ASSERT_TRUE(rewind.render);
  EXPECT_EQ(groupText(rewind), groups[2]);
}

TEST(RsvpPresentationGroup, RestoreValidatesLastGroupTokenAndContinuesAfterIt) {
  Source source({word("сделал", 10), word("бы", 17), word("домой", 20), marker(rsvp::EventKind::EndOfBook, 26)});
  rsvp::RsvpSession session(source, {.visibleTextOffset = 17, .valid = true}, {}, true, acceptAll, nullptr);
  session.restoreAfterCheckpoint(hash("бы", static_cast<uint16_t>(std::strlen("бы"))),
                                 static_cast<uint16_t>(std::strlen("бы")));
  const auto restored = session.step({});
  ASSERT_TRUE(restored.render);
  EXPECT_EQ(groupText(restored), (std::vector<std::string>{"домой"}));
}

TEST(RsvpPresentationGroup, BidirectionalAndExperimentalExamplesKeepActiveWordAtEnd) {
  for (const auto& words :
       {std::vector<const char*>{"я", "бы", "сделал"}, std::vector<const char*>{"а", "он", "пришёл"}}) {
    Source source({word(words[0], 0), word(words[1], 3), word(words[2], 7), marker(rsvp::EventKind::EndOfBook, 15)});
    rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
    const auto frame = session.step({});
    ASSERT_EQ(groupText(frame), (std::vector<std::string>{words[0], words[1], words[2]}));
    ASSERT_NE(frame.frame.presentationGroup, nullptr);
    EXPECT_EQ(frame.frame.presentationGroup->activeIndex, 2u);
  }
}

TEST(RsvpPresentationGroup, AmbiguousStandaloneFormsDoNotBecomeCompanions) {
  Source source({word("что", 0), word("произошло", 4), word("это", 14), word("сделал", 18), word("я", 25),
                 marker(rsvp::EventKind::EndOfBook, 28)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);

  const auto first = session.step({});
  EXPECT_EQ(groupText(first), (std::vector<std::string>{"что"}));
  acknowledge(session, first);
  const auto second = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(second), (std::vector<std::string>{"произошло"}));
  acknowledge(session, second);
  const auto third = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(third), (std::vector<std::string>{"это"}));
  acknowledge(session, third);
  const auto fourth = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(fourth), (std::vector<std::string>{"сделал"}));
  acknowledge(session, fourth);
  const auto fifth = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(groupText(fifth), (std::vector<std::string>{"я"}));
}

TEST(RsvpPresentationGroup, TwoPostfixCompanionsAreRetainedAsOneGroup) {
  Source source({word("сделал", 0), word("бы", 7), word("же", 12), marker(rsvp::EventKind::EndOfBook, 16)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  EXPECT_EQ(groupText(frame), (std::vector<std::string>{"сделал", "бы", "же"}));
  EXPECT_EQ(frame.frame.presentationGroup->activeIndex, 0u);
}

TEST(RsvpPresentationGroup, TwoCompanionsUseOneThousandTwentyMillisecondInterval) {
  Source source({word("сделал", 0), word("бы", 7), word("же", 12), marker(rsvp::EventKind::EndOfBook, 16)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_TRUE(frame.render);
  acknowledge(session, frame, 0);
  const auto playing = session.step({.nowMs = 0, .action = rsvp::Action::TogglePlayback});
  EXPECT_EQ(playing.nextDeadlineMs, 1020u);
}

TEST(RsvpPresentationGroup, PunctuationOnLastCompanionDrivesPauseClass) {
  Source source({word("сделал", 0), word("бы.", 7), marker(rsvp::EventKind::EndOfBook, 11)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_EQ(groupText(frame), (std::vector<std::string>{"сделал", "бы."}));
  acknowledge(session, frame, 0);
  const auto playing = session.step({.nowMs = 0, .action = rsvp::Action::TogglePlayback});
  EXPECT_EQ(playing.nextDeadlineMs, 1410u);
}

TEST(RsvpPresentationGroup, RestoreMismatchFallsBackToInvalidDocument) {
  Source source({word("сделал", 10), word("бы", 17), marker(rsvp::EventKind::EndOfBook, 20)});
  rsvp::RsvpSession session(source, {.visibleTextOffset = 17, .valid = true}, {}, true, acceptAll, nullptr);
  session.restoreAfterCheckpoint(hash("нет", 6), 6);
  const auto decision = session.step({});
  EXPECT_EQ(decision.state, rsvp::State::Error);
  EXPECT_EQ(decision.error, rsvp::Error::InvalidDocument);
}

TEST(RsvpPresentationGroup, RestoringLastGroupAtEndOfBookFinishesAfterSkip) {
  Source source({word("сделал", 10), word("бы", 17), marker(rsvp::EventKind::EndOfBook, 20)});
  rsvp::RsvpSession session(source, {.visibleTextOffset = 17, .valid = true}, {}, true, acceptAll, nullptr);
  session.restoreAfterCheckpoint(hash("бы", static_cast<uint16_t>(std::strlen("бы"))),
                                 static_cast<uint16_t>(std::strlen("бы")));
  const auto decision = session.step({});
  EXPECT_EQ(decision.state, rsvp::State::Finished);
  EXPECT_FALSE(decision.render);
}

TEST(RsvpPresentationGroup, UnacknowledgedFrameDoesNotAdvanceDurablePosition) {
  Source source({word("сделал", 0), word("бы", 7), word("домой", 12), marker(rsvp::EventKind::EndOfBook, 18)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_TRUE(frame.render);
  EXPECT_FALSE(session.currentAnchor().valid);
  const auto next = session.step({.action = rsvp::Action::StepForward});
  EXPECT_FALSE(next.presentationAccepted);
  EXPECT_FALSE(session.currentAnchor().valid);
}

TEST(RsvpPresentationGroup, DecomposedTokenUsesNormalizedIdentityForCheckpointHash) {
  Source source({word("e\xCC\x81", 0), marker(rsvp::EventKind::EndOfBook, 5)});
  rsvp::RsvpSession session(source, {}, {}, true, acceptAll, nullptr);
  const auto frame = session.step({});
  ASSERT_TRUE(frame.render);
  acknowledge(session, frame);
  const char normalized[] = "\xC3\xA9";
  EXPECT_EQ(session.currentTokenHash(), hash(normalized, static_cast<uint16_t>(std::strlen(normalized))));
  EXPECT_EQ(session.currentTokenLength(), static_cast<uint16_t>(std::strlen(normalized)));
}

}  // namespace
