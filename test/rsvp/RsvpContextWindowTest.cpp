#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "RsvpSession.h"

namespace {

rsvp::DocumentEvent event(const rsvp::EventKind kind, const char* text = nullptr, const uint32_t offset = 0,
                          const rsvp::NonTextKind nonText = rsvp::NonTextKind::None) {
  rsvp::DocumentEvent out;
  out.kind = kind;
  out.anchor = {.spineIndex = 0, .visibleTextOffset = offset, .sameOffsetOrdinal = 0, .valid = true};
  out.nonText = nonText;
  if (text != nullptr) {
    out.textLength = static_cast<uint16_t>(std::strlen(text));
    EXPECT_LE(out.textLength, rsvp::MAX_TOKEN_BYTES);
    std::memcpy(out.text, text, out.textLength + 1);
  }
  return out;
}

rsvp::DocumentEvent word(const char* text, const uint32_t offset) {
  return event(rsvp::EventKind::Word, text, offset);
}

rsvp::DocumentEvent punctuation(const char* text, const uint32_t offset) {
  return event(rsvp::EventKind::NonLexicalText, text, offset);
}

rsvp::DocumentEvent boundary(const rsvp::EventKind kind, const uint32_t offset) {
  return event(kind, nullptr, offset);
}

rsvp::DocumentEvent nonText(const rsvp::NonTextKind kind, const uint32_t offset) {
  return event(rsvp::EventKind::NonText, nullptr, offset, kind);
}

class CountingSource final : public rsvp::RsvpSource {
 public:
  explicit CountingSource(std::vector<rsvp::DocumentEvent> events) : events(std::move(events)) {}

  bool open(const rsvp::ResumeAnchor* anchor) override {
    ++openCalls;
    index = 0;
    if (anchor == nullptr || !anchor->valid) return true;

    uint16_t ordinal = 0;
    for (size_t candidate = 0; candidate < events.size(); ++candidate) {
      const auto& candidateEvent = events[candidate];
      if (candidateEvent.anchor.visibleTextOffset < anchor->visibleTextOffset) continue;
      if (candidateEvent.anchor.visibleTextOffset == anchor->visibleTextOffset &&
          ordinal++ != anchor->sameOffsetOrdinal)
        continue;
      index = candidate;
      return true;
    }
    index = events.size();
    return true;
  }

  bool next(rsvp::DocumentEvent& out) override {
    ++nextCalls;
    if (index >= events.size()) return false;
    out = events[index++];
    return true;
  }

  std::vector<rsvp::DocumentEvent> events;
  size_t index = 0;
  size_t openCalls = 0;
  size_t nextCalls = 0;
};

std::vector<std::string> contextTexts(const rsvp::Decision& decision) {
  if (decision.frame.contextWindow == nullptr) {
    ADD_FAILURE() << "expected an enabled Context Window";
    return {};
  }
  std::vector<std::string> texts;
  texts.reserve(decision.frame.contextWindow->count);
  for (uint8_t index = 0; index < decision.frame.contextWindow->count; ++index) {
    texts.emplace_back(decision.frame.contextWindow->tokens[index].text);
  }
  return texts;
}

void acknowledge(rsvp::RsvpSession& session, const rsvp::Decision& frame, const uint32_t nowMs = 0) {
  ASSERT_TRUE(session.step({.nowMs = nowMs,
                            .action = rsvp::Action::FramePresented,
                            .presentedFrameId = frame.frame.id})
                  .presentationAccepted);
}

TEST(RsvpContextWindow, DefaultSessionDoesNotExposeContextWindow) {
  CountingSource source({word("active", 0), word("following", 7), boundary(rsvp::EventKind::EndOfBook, 16)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});

  ASSERT_TRUE(first.render);
  EXPECT_EQ(first.frame.contextWindow, nullptr);
}

TEST(RsvpContextWindow, EnabledWindowProvidesPreviousFollowingAndStandalonePunctuation) {
  CountingSource source({word("previous", 0), punctuation(",", 8), word("active", 10), punctuation(";", 17),
                         word("following", 19), boundary(rsvp::EventKind::EndOfBook, 29)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});
  acknowledge(session, first);
  const auto active = session.step({.action = rsvp::Action::StepForward});

  ASSERT_TRUE(active.render);
  ASSERT_EQ(active.frame.contextWindow->activeIndex, 2u);
  EXPECT_EQ(contextTexts(active), (std::vector<std::string>{"previous", ",", "active", ";", "following"}));
}

TEST(RsvpContextWindow, ReadAheadDoesNotAdvanceDurableAnchorCheckpointOrDeadline) {
  CountingSource source({word("one", 0), word("two", 4), word("three", 8), boundary(rsvp::EventKind::EndOfBook, 14)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({.nowMs = 0});
  ASSERT_TRUE(first.render);
  EXPECT_FALSE(session.currentAnchor().valid);
  EXPECT_FALSE(first.checkpointRequested);
  acknowledge(session, first, 300);
  const auto playing = session.step({.nowMs = 300, .action = rsvp::Action::TogglePlayback});
  const auto second = session.step({.nowMs = playing.nextDeadlineMs});

  ASSERT_TRUE(second.render);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 0u);
  EXPECT_FALSE(second.checkpointRequested);
  EXPECT_EQ(second.nextDeadlineMs, 0u);
  EXPECT_EQ(session.requestedAnchor().visibleTextOffset, 4u);
}

TEST(RsvpContextWindow, ContextStopsBeforeSentenceParagraphChapterAndNonTextBoundaries) {
  const std::vector<std::pair<rsvp::EventKind, rsvp::DocumentEvent>> cases = {
      {rsvp::EventKind::NonLexicalText, punctuation(".", 6)},
      {rsvp::EventKind::ParagraphBoundary, boundary(rsvp::EventKind::ParagraphBoundary, 6)},
      {rsvp::EventKind::ChapterBoundary, boundary(rsvp::EventKind::ChapterBoundary, 6)},
  };
  for (const auto& [kind, marker] : cases) {
    SCOPED_TRACE(static_cast<int>(kind));
    CountingSource source({word("active", 0), marker, word("after", 8), boundary(rsvp::EventKind::EndOfBook, 14)});
    rsvp::RsvpSession session(source, {}, {}, true);

    const auto first = session.step({});

    ASSERT_TRUE(first.render);
    EXPECT_EQ(contextTexts(first), (std::vector<std::string>{"active"}));
  }

  CountingSource source({word("active", 0), nonText(rsvp::NonTextKind::Image, 7), word("after", 8),
                         boundary(rsvp::EventKind::EndOfBook, 14)});
  rsvp::RsvpSession session(source, {}, {}, true);
  const auto first = session.step({});
  ASSERT_TRUE(first.render);
  EXPECT_EQ(contextTexts(first), (std::vector<std::string>{"active"}));
}

TEST(RsvpContextWindow, ClosingQuotesDoNotLeakTheNextSentenceIntoContext) {
  CountingSource source({word("active.\"", 0), word("after", 9), boundary(rsvp::EventKind::EndOfBook, 15)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});

  ASSERT_TRUE(first.render);
  EXPECT_EQ(contextTexts(first), (std::vector<std::string>{"active.\""}));
}

TEST(RsvpContextWindow, AbbreviationPeriodDoesNotEndTheContextSentence) {
  CountingSource source({word("e.g.", 0), word("example", 5), boundary(rsvp::EventKind::EndOfBook, 13)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});

  ASSERT_TRUE(first.render);
  EXPECT_EQ(contextTexts(first), (std::vector<std::string>{"e.g.", "example"}));
}

TEST(RsvpContextWindow, ParagraphBeforeChapterStillSurfacesTheChapterPause) {
  CountingSource source({word("last", 0), boundary(rsvp::EventKind::ParagraphBoundary, 5),
                         boundary(rsvp::EventKind::ChapterBoundary, 5), word("next", 6),
                         boundary(rsvp::EventKind::EndOfBook, 11)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});
  acknowledge(session, first);
  session.step({.action = rsvp::Action::TogglePlayback});
  const auto pause = session.step({.nowMs = 1500});

  EXPECT_EQ(pause.state, rsvp::State::Paused);
  EXPECT_EQ(pause.pauseReason, rsvp::PauseReason::Chapter);
  EXPECT_EQ(pause.frame.contextWindow, nullptr);
}

TEST(RsvpContextWindow, FutureSourceErrorIsDeferredUntilPlaybackReachesIt) {
  CountingSource source({word("safe", 0), boundary(rsvp::EventKind::Error, 5)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});
  ASSERT_TRUE(first.render);
  EXPECT_EQ(first.state, rsvp::State::Paused);
  EXPECT_EQ(contextTexts(first), (std::vector<std::string>{"safe"}));
  acknowledge(session, first);

  const auto error = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(error.state, rsvp::State::Error);
  EXPECT_FALSE(error.render);
}

TEST(RsvpContextWindow, PausedWindowIsStableAndDoesNotReadTheSourceAgain) {
  CountingSource source({word("one", 0), word("two", 4), word("three", 8), boundary(rsvp::EventKind::EndOfBook, 14)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});
  ASSERT_TRUE(first.render);
  const auto readsAfterRender = source.nextCalls;
  const auto repeated = session.step({.nowMs = 100});

  EXPECT_EQ(contextTexts(repeated), contextTexts(first));
  EXPECT_EQ(source.nextCalls, readsAfterRender);
}

TEST(RsvpContextWindow, StepAndRewindRecomputeTheWindowAroundTheActiveWord) {
  CountingSource source({word("one", 0), word("two", 4), word("three", 8), word("four", 14),
                         boundary(rsvp::EventKind::EndOfBook, 19)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto first = session.step({});
  acknowledge(session, first);
  const auto second = session.step({.action = rsvp::Action::StepForward});
  acknowledge(session, second);
  const auto third = session.step({.action = rsvp::Action::StepForward});
  ASSERT_TRUE(third.render);
  EXPECT_EQ(third.frame.contextWindow->tokens[third.frame.contextWindow->activeIndex].text, std::string("three"));
  EXPECT_EQ(contextTexts(third), (std::vector<std::string>{"one", "two", "three", "four"}));
  acknowledge(session, third);

  const auto rewind = session.step({.action = rsvp::Action::RewindFive});
  ASSERT_TRUE(rewind.render);
  EXPECT_EQ(rewind.frame.contextWindow->tokens[rewind.frame.contextWindow->activeIndex].text, std::string("one"));
  EXPECT_EQ(contextTexts(rewind), (std::vector<std::string>{"one", "two", "three", "four"}));
}

TEST(RsvpContextWindow, CapacityRetainsNearestTokensFirst) {
  std::vector<rsvp::DocumentEvent> events;
  for (uint32_t index = 0; index < 9; ++index) {
    const std::string text = "word" + std::to_string(index);
    events.push_back(word(text.c_str(), index * 4));
  }
  events.push_back(boundary(rsvp::EventKind::EndOfBook, 40));
  CountingSource source(std::move(events));
  rsvp::RsvpSession session(source, {}, {}, true);

  auto frame = session.step({});
  acknowledge(session, frame);
  for (int index = 0; index < 4; ++index) {
    frame = session.step({.action = rsvp::Action::StepForward});
    ASSERT_TRUE(frame.render);
    acknowledge(session, frame);
  }
  const auto texts = contextTexts(frame);

  EXPECT_LE(texts.size(), 7u);
  EXPECT_NE(std::find(texts.begin(), texts.end(), "word1"), texts.end());
  EXPECT_NE(std::find(texts.begin(), texts.end(), "word3"), texts.end());
  EXPECT_NE(std::find(texts.begin(), texts.end(), "word5"), texts.end());
  EXPECT_NE(std::find(texts.begin(), texts.end(), "word7"), texts.end());
  EXPECT_EQ(std::find(texts.begin(), texts.end(), "word0"), texts.end());
  EXPECT_EQ(std::find(texts.begin(), texts.end(), "word8"), texts.end());
}

TEST(RsvpContextWindow, Utf8PoolExhaustionKeepsActiveAndNearestCompleteWords) {
  std::string maximumUtf8Word;
  maximumUtf8Word.reserve(rsvp::MAX_TOKEN_BYTES);
  for (size_t index = 0; index < rsvp::MAX_TOKEN_BYTES / 2; ++index) maximumUtf8Word += "я";
  CountingSource source({word("active", 0), word(maximumUtf8Word.c_str(), 7), word(maximumUtf8Word.c_str(), 208),
                         word(maximumUtf8Word.c_str(), 409), boundary(rsvp::EventKind::EndOfBook, 610)});
  rsvp::RsvpSession session(source, {}, {}, true);

  const auto frame = session.step({});

  ASSERT_TRUE(frame.render);
  ASSERT_NE(frame.frame.contextWindow, nullptr);
  EXPECT_EQ(frame.frame.contextWindow->activeIndex, 0u);
  ASSERT_EQ(frame.frame.contextWindow->count, 3u);
  EXPECT_STREQ(frame.frame.contextWindow->tokens[0].text, "active");
  EXPECT_EQ(frame.frame.contextWindow->tokens[1].textLength, rsvp::MAX_TOKEN_BYTES);
  EXPECT_EQ(frame.frame.contextWindow->tokens[2].textLength, rsvp::MAX_TOKEN_BYTES);
}

}  // namespace
