#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "RsvpSession.h"

namespace {

rsvp::DocumentEvent word(const char* text, const uint32_t offset, const uint16_t ordinal = 0) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::Word;
  event.anchor = {.visibleTextOffset = offset, .sameOffsetOrdinal = ordinal, .valid = true};
  event.textLength = static_cast<uint16_t>(strlen(text));
  memcpy(event.text, text, event.textLength + 1);
  return event;
}

rsvp::DocumentEvent marker(const rsvp::EventKind kind) {
  rsvp::DocumentEvent event;
  event.kind = kind;
  event.anchor.valid = true;
  return event;
}

uint32_t fnv1a(const char* text, const uint16_t length) {
  uint32_t hash = 2166136261U;
  for (uint16_t index = 0; index < length; ++index) {
    hash ^= static_cast<uint8_t>(text[index]);
    hash *= 16777619U;
  }
  return hash;
}

class VectorSource final : public rsvp::RsvpSource {
 public:
  explicit VectorSource(std::vector<rsvp::DocumentEvent> events) : events(std::move(events)) {}

  bool open(const rsvp::ResumeAnchor* anchor) override {
    index = 0;
    if (anchor == nullptr || !anchor->valid) return true;

    uint16_t ordinal = 0;
    for (size_t candidate = 0; candidate < events.size(); candidate++) {
      const auto& event = events[candidate];
      if (event.kind == rsvp::EventKind::EndOfBook) continue;
      if (event.anchor.visibleTextOffset < anchor->visibleTextOffset) continue;
      if (event.anchor.visibleTextOffset == anchor->visibleTextOffset) {
        if (ordinal++ != anchor->sameOffsetOrdinal) continue;
      }
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

 private:
  std::vector<rsvp::DocumentEvent> events;
  size_t index = 0;
};

std::string frameText(const rsvp::Decision& decision) {
  if (!decision.render || decision.frame.text == nullptr) return {};
  return {decision.frame.text, decision.frame.textLength};
}

void acknowledge(rsvp::RsvpSession& session, const rsvp::Decision& frame, const uint32_t nowMs,
                 const uint32_t refreshDurationMs = 0) {
  const auto acknowledgement = session.step({.nowMs = nowMs,
                                             .action = rsvp::Action::FramePresented,
                                             .presentedFrameId = frame.frame.id,
                                             .refreshDurationMs = refreshDurationMs});
  ASSERT_TRUE(acknowledgement.presentationAccepted);
}

TEST(RsvpSessionPlayback, RefreshConsumesBaseIntervalAndLateClockNeverSkips) {
  VectorSource source({word("one", 0), word("two", 4), word("three", 8), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 0});
  ASSERT_EQ(frameText(first), "one");
  acknowledge(session, first, 300, 300);

  const auto playing = session.step({.nowMs = 300, .action = rsvp::Action::TogglePlayback});
  EXPECT_EQ(playing.state, rsvp::State::Playing);
  EXPECT_EQ(playing.nextDeadlineMs, 600u);

  EXPECT_FALSE(session.step({.nowMs = 599}).render);
  const auto second = session.step({.nowMs = 600});
  EXPECT_EQ(frameText(second), "two");
  acknowledge(session, second, 700, 100);

  const auto late = session.step({.nowMs = 5000});
  EXPECT_EQ(frameText(late), "three");
}

TEST(RsvpSessionPlayback, RepeatedPresentationAcknowledgementDoesNotMoveTheDeadline) {
  VectorSource source({word("one", 0), word("two", 4), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 0});
  acknowledge(session, first, 100, 100);
  const auto repeated = session.step({.nowMs = 400,
                                      .action = rsvp::Action::FramePresented,
                                      .presentedFrameId = first.frame.id,
                                      .refreshDurationMs = 300});

  EXPECT_TRUE(repeated.presentationAccepted);
  EXPECT_EQ(repeated.nextDeadlineMs, 600u);
}

TEST(RsvpSessionPlayback, LongRefreshConsumesTheWholePunctuationInterval) {
  VectorSource source({word("конец!", 0), word("дальше", 10), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 0});
  acknowledge(session, first, 1300, 1300);
  const auto playing = session.step({.nowMs = 1300, .action = rsvp::Action::TogglePlayback});

  EXPECT_EQ(playing.nextDeadlineMs, 1300u);
  EXPECT_EQ(frameText(session.step({.nowMs = 1300})), "дальше");
}

TEST(RsvpSessionPlayback, PaceIsTenWpmAndSafelyClamped) {
  VectorSource source({word("one", 0), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  EXPECT_EQ(session.step({}).paceWpm, 100u);
  EXPECT_EQ(session.step({.action = rsvp::Action::PaceDown}).paceWpm, 90u);
  for (int i = 0; i < 10; i++) session.step({.action = rsvp::Action::PaceDown});
  EXPECT_EQ(session.step({}).paceWpm, 60u);
  for (int i = 0; i < 10; i++) session.step({.action = rsvp::Action::PaceUp});
  EXPECT_EQ(session.step({}).paceWpm, 100u);

  VectorSource fasterSource({word("one", 0), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpPacingConfig fasterPanel;
  fasterPanel.safeMaximumWpm = 120;
  rsvp::RsvpSession fasterSession(fasterSource, {}, fasterPanel);
  for (int i = 0; i < 10; i++) fasterSession.step({.action = rsvp::Action::PaceUp});
  EXPECT_EQ(fasterSession.step({}).paceWpm, 120u);
}

TEST(RsvpSessionPlayback, PunctuationAndParagraphExtendTheDeadline) {
  VectorSource source({word("слово,", 0), word("конец.»", 7), marker(rsvp::EventKind::ParagraphBoundary),
                       word("новый", 15), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 0});
  acknowledge(session, first, 0);
  session.step({.nowMs = 0, .action = rsvp::Action::TogglePlayback});
  EXPECT_FALSE(session.step({.nowMs = 899}).render);
  const auto second = session.step({.nowMs = 900});
  EXPECT_EQ(frameText(second), "конец.»");
  acknowledge(session, second, 900);

  EXPECT_FALSE(session.step({.nowMs = 2399}).render);
  const auto third = session.step({.nowMs = 2400});
  EXPECT_EQ(frameText(third), "новый");
}

TEST(RsvpSessionPlayback, ParagraphBoundaryRequestsPeriodicCleanupRefresh) {
  VectorSource source({word("конец", 0), marker(rsvp::EventKind::ParagraphBoundary), word("новый", 6),
                       marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpPacingConfig pacing;
  pacing.cleanupEveryFrames = 1;
  rsvp::RsvpSession session(source, {}, pacing);

  const auto first = session.step({});

  EXPECT_TRUE(first.render);
  EXPECT_TRUE(first.cleanupRefresh);
}

TEST(RsvpSessionPlayback, ChapterBoundaryPausesBeforeTheNextWord) {
  VectorSource source(
      {word("one", 0), marker(rsvp::EventKind::ChapterBoundary), word("two", 4), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  session.step({.action = rsvp::Action::TogglePlayback});

  const auto boundary = session.step({.nowMs = 600});
  EXPECT_EQ(boundary.state, rsvp::State::Paused);
  EXPECT_EQ(boundary.pauseReason, rsvp::PauseReason::Chapter);
  EXPECT_FALSE(boundary.render);
  EXPECT_FALSE(boundary.switchToPaged);

  const auto next = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(frameText(next), "two");
}

TEST(RsvpSessionPlayback, UnsupportedElementsOfferPagedFallbackWithReason) {
  auto image = marker(rsvp::EventKind::NonText);
  image.nonText = rsvp::NonTextKind::Image;
  VectorSource source({word("one", 0), image, marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  session.step({.action = rsvp::Action::TogglePlayback});

  const auto decision = session.step({.nowMs = 600});
  EXPECT_EQ(decision.state, rsvp::State::Boundary);
  EXPECT_EQ(decision.pauseReason, rsvp::PauseReason::Image);
  EXPECT_TRUE(decision.pagedModeAvailable);
  EXPECT_FALSE(decision.switchToPaged);
}

TEST(RsvpSessionPlayback, PageForwardSkipsEachNonTextBoundaryAndShowsTheNextWordPaused) {
  constexpr rsvp::NonTextKind kinds[] = {rsvp::NonTextKind::Image, rsvp::NonTextKind::Table,
                                         rsvp::NonTextKind::HorizontalRule, rsvp::NonTextKind::Other};
  for (const auto kind : kinds) {
    SCOPED_TRACE(static_cast<int>(kind));
    auto nonText = marker(rsvp::EventKind::NonText);
    nonText.nonText = kind;
    VectorSource source({word("one", 0), nonText, word("two", 4), marker(rsvp::EventKind::EndOfBook)});
    rsvp::RsvpSession session(source);

    const auto first = session.step({});
    acknowledge(session, first, 0);
    session.step({.action = rsvp::Action::TogglePlayback});
    const auto boundary = session.step({.nowMs = 600});
    ASSERT_EQ(boundary.state, rsvp::State::Boundary);

    const auto next = session.step({.nowMs = 600, .action = rsvp::Action::StepForward});
    EXPECT_EQ(frameText(next), "two");
    EXPECT_EQ(next.state, rsvp::State::Paused);
    EXPECT_EQ(next.pauseReason, rsvp::PauseReason::None);
    EXPECT_FALSE(next.pagedModeAvailable);
    EXPECT_TRUE(next.checkpointRequested);
  }
}

TEST(RsvpSessionPlayback, PageForwardSkipsOneConsecutiveNonTextBoundaryPerPress) {
  auto image = marker(rsvp::EventKind::NonText);
  image.nonText = rsvp::NonTextKind::Image;
  auto table = marker(rsvp::EventKind::NonText);
  table.nonText = rsvp::NonTextKind::Table;
  VectorSource source({word("one", 0), image, table, word("two", 4), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  session.step({.action = rsvp::Action::TogglePlayback});
  ASSERT_EQ(session.step({.nowMs = 600}).pauseReason, rsvp::PauseReason::Image);

  const auto tableBoundary = session.step({.nowMs = 600, .action = rsvp::Action::StepForward});
  EXPECT_FALSE(tableBoundary.render);
  EXPECT_EQ(tableBoundary.state, rsvp::State::Boundary);
  EXPECT_EQ(tableBoundary.pauseReason, rsvp::PauseReason::Table);
  EXPECT_EQ(tableBoundary.nextDeadlineMs, 0u);

  const auto next = session.step({.nowMs = 600, .action = rsvp::Action::StepForward});
  EXPECT_EQ(frameText(next), "two");
  EXPECT_EQ(next.state, rsvp::State::Paused);
}

TEST(RsvpSessionPlayback, FrameCarriesNormalizedUnicodeOrpPreparation) {
  constexpr char rawWord[] = "\xD0\xB5\xCC\x88-\xD0\xBA\xD0\xBE\xD1\x82";
  VectorSource source({word(rawWord, 0), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto decision = session.step({});

  ASSERT_TRUE(decision.render);
  ASSERT_NE(decision.frame.preparedWord, nullptr);
  EXPECT_STREQ(decision.frame.text, "\xD1\x91-\xD0\xBA\xD0\xBE\xD1\x82");
  const auto& prepared = *decision.frame.preparedWord;
  EXPECT_EQ(std::string(prepared.text + prepared.pivot.begin, prepared.pivot.size()), "\xD0\xBA");
  EXPECT_EQ(session.currentTokenLength(), prepared.textLength);
  EXPECT_EQ(session.currentTokenHash(), fnv1a(prepared.text, prepared.textLength));
  EXPECT_NE(session.currentTokenHash(), fnv1a(rawWord, sizeof(rawWord) - 1));
}

TEST(RsvpSessionPlayback, PixelOverflowPausesWithPagedModeAvailable) {
  VectorSource source({word("unrenderable", 0), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  const auto boundary = session.step({.action = rsvp::Action::WordDoesNotFit});

  EXPECT_EQ(boundary.state, rsvp::State::Boundary);
  EXPECT_EQ(boundary.pauseReason, rsvp::PauseReason::OversizedWord);
  EXPECT_TRUE(boundary.pagedModeAvailable);
  EXPECT_FALSE(boundary.switchToPaged);
  const auto ignoredStep = session.step({.action = rsvp::Action::StepForward});
  EXPECT_FALSE(ignoredStep.render);
  EXPECT_EQ(ignoredStep.state, rsvp::State::Boundary);
  EXPECT_EQ(ignoredStep.pauseReason, rsvp::PauseReason::OversizedWord);
  EXPECT_TRUE(session.step({.action = rsvp::Action::ModeSwitch}).switchToPaged);
}

TEST(RsvpSessionPlayback, StepForwardAndRewindUseBoundedHistory) {
  std::vector<rsvp::DocumentEvent> events;
  for (uint32_t offset = 0; offset < 7; offset++) {
    char text[2] = {static_cast<char>('1' + offset), '\0'};
    events.push_back(word(text, offset));
  }
  events.push_back(marker(rsvp::EventKind::EndOfBook));
  VectorSource source(std::move(events));
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  EXPECT_EQ(frameText(first), "1");
  acknowledge(session, first, 0);
  for (int i = 0; i < 6; i++) {
    const auto next = session.step({.action = rsvp::Action::StepForward});
    EXPECT_TRUE(next.render);
    acknowledge(session, next, static_cast<uint32_t>(i + 1));
  }
  const auto rewind = session.step({.action = rsvp::Action::RewindFive});
  EXPECT_EQ(frameText(rewind), "2");
  EXPECT_EQ(rewind.state, rsvp::State::Paused);
}

TEST(RsvpSessionPlayback, ResumeAnchorDisambiguatesSameOffsetOrdinal) {
  VectorSource source({word("first", 4, 0), word("second", 4, 1), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source, {.visibleTextOffset = 4, .sameOffsetOrdinal = 1, .valid = true});

  const auto decision = session.step({});
  EXPECT_EQ(frameText(decision), "second");
}

TEST(RsvpSessionPlayback, CheckpointPolicyCoversPeriodicPauseAndModeSwitch) {
  VectorSource source({word("one", 7), word("two", 11), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 100});
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 7u);
  EXPECT_FALSE(first.checkpointRequested);
  acknowledge(session, first, 200);

  EXPECT_FALSE(session.step({.nowMs = 200, .action = rsvp::Action::TogglePlayback}).checkpointRequested);
  const auto second = session.step({.nowMs = 30199});
  EXPECT_FALSE(second.checkpointRequested);
  const auto periodic =
      session.step({.nowMs = 30200, .action = rsvp::Action::FramePresented, .presentedFrameId = second.frame.id});
  EXPECT_TRUE(periodic.presentationAccepted);
  EXPECT_TRUE(periodic.checkpointRequested);

  const auto paused = session.step({.nowMs = 30300, .action = rsvp::Action::TogglePlayback});
  EXPECT_EQ(paused.state, rsvp::State::Paused);
  EXPECT_TRUE(paused.checkpointRequested);

  const auto switched = session.step({.nowMs = 30400, .action = rsvp::Action::ModeSwitch});
  EXPECT_TRUE(switched.checkpointRequested);
  EXPECT_TRUE(switched.switchToPaged);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 11u);
}

TEST(RsvpSessionPlayback, ManualPausedNavigationRequestsCheckpoint) {
  VectorSource source({word("one", 0), word("two", 4), word("three", 8), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  const auto forward = session.step({.action = rsvp::Action::StepForward});
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 4U);
  EXPECT_TRUE(forward.checkpointRequested);
  acknowledge(session, forward, 0);

  const auto rewind = session.step({.action = rsvp::Action::RewindFive});
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 0U);
  EXPECT_TRUE(rewind.checkpointRequested);
}

TEST(RsvpSessionPlayback, ActiveReadingTimeExcludesPausedTime) {
  VectorSource source({word("one", 0), word("two", 4), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({.nowMs = 100});
  acknowledge(session, first, 200);
  session.step({.nowMs = 300, .action = rsvp::Action::TogglePlayback});
  const auto second = session.step({.nowMs = 1300});
  acknowledge(session, second, 1400);
  session.step({.nowMs = 1800, .action = rsvp::Action::TogglePlayback});
  session.step({.nowMs = 5000});

  EXPECT_EQ(session.activeReadingMs(), 1500u);
}

TEST(RsvpSessionPlayback, FatalSourceErrorReturnsToPagedAtLastSafeAnchor) {
  VectorSource source({word("safe", 42), marker(rsvp::EventKind::Error)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  session.step({.action = rsvp::Action::TogglePlayback});
  const auto failure = session.step({.nowMs = 600});

  EXPECT_EQ(failure.state, rsvp::State::Error);
  EXPECT_TRUE(failure.switchToPaged);
  EXPECT_TRUE(failure.checkpointRequested);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 42u);
}

TEST(RsvpSessionPlayback, InvalidNextTokenKeepsTheLastSuccessfullyDisplayedAnchor) {
  VectorSource source({word("safe", 42), word("!!!", 99), marker(rsvp::EventKind::EndOfBook)});
  rsvp::RsvpSession session(source);

  const auto first = session.step({});
  acknowledge(session, first, 0);
  session.step({.action = rsvp::Action::TogglePlayback});
  const auto failure = session.step({.nowMs = 600});

  EXPECT_EQ(failure.state, rsvp::State::Error);
  EXPECT_EQ(failure.error, rsvp::Error::InvalidDocument);
  EXPECT_EQ(session.currentAnchor().visibleTextOffset, 42U);
  EXPECT_EQ(session.currentTokenLength(), 4U);
}

}  // namespace
