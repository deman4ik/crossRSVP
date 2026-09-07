#include "RsvpSession.h"

#include <algorithm>
#include <cstring>

#include "RsvpCompanionPolicy.h"

namespace rsvp {

namespace {

uint16_t clampPace(const uint16_t pace, const RsvpPacingConfig& config) {
  const uint16_t maximum = std::min(config.maximumWpm, config.safeMaximumWpm);
  return std::min(maximum, std::max(config.minimumWpm, pace));
}

bool isClosingCodepoint(const uint32_t codepoint) {
  return codepoint == ')' || codepoint == ']' || codepoint == '}' || codepoint == '>' || codepoint == '\'' ||
         codepoint == '"' || codepoint == 0x00BB || codepoint == 0x2019 || codepoint == 0x201D || codepoint == 0x203A ||
         codepoint == 0x3009 || codepoint == 0x300B || codepoint == 0x300D || codepoint == 0x300F ||
         codepoint == 0x3011 || codepoint == 0x3015 || codepoint == 0x3017 || codepoint == 0x3019 ||
         codepoint == 0x301B;
}

bool isSkippableNonTextBoundary(const PauseReason reason) {
  return reason == PauseReason::Image || reason == PauseReason::Table || reason == PauseReason::HorizontalRule ||
         reason == PauseReason::OtherContent;
}

bool isSentenceTerminal(const uint32_t codepoint) {
  return codepoint == '.' || codepoint == '!' || codepoint == '?' || codepoint == 0x2026;
}

uint32_t nextCodepoint(const char* text, const uint16_t length, uint16_t& offset) {
  const uint8_t first = static_cast<uint8_t>(text[offset]);
  uint32_t codepoint = first;
  uint16_t width = 1;
  if ((first & 0xE0) == 0xC0 && offset + 1 < length) {
    codepoint = static_cast<uint32_t>(first & 0x1F) << 6;
    codepoint |= static_cast<uint8_t>(text[offset + 1]) & 0x3F;
    width = 2;
  } else if ((first & 0xF0) == 0xE0 && offset + 2 < length) {
    codepoint = static_cast<uint32_t>(first & 0x0F) << 12;
    codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[offset + 1]) & 0x3F) << 6;
    codepoint |= static_cast<uint8_t>(text[offset + 2]) & 0x3F;
    width = 3;
  } else if ((first & 0xF8) == 0xF0 && offset + 3 < length) {
    codepoint = static_cast<uint32_t>(first & 0x07) << 18;
    codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[offset + 1]) & 0x3F) << 12;
    codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[offset + 2]) & 0x3F) << 6;
    codepoint |= static_cast<uint8_t>(text[offset + 3]) & 0x3F;
    width = 4;
  }
  offset = static_cast<uint16_t>(offset + width);
  return codepoint;
}

}  // namespace

RsvpSession::RsvpSession(RsvpSource& source, const ResumeAnchor initialAnchor, const RsvpPacingConfig pacing,
                         const bool groupingEnabled, PresentationGroupFitCallback fitCallback, void* fitContext)
    : source(source),
      initialAnchor(initialAnchor),
      pacing(pacing),
      groupingEnabled(groupingEnabled),
      fitCallback(fitCallback),
      fitContext(fitContext),
      paceWpm(clampPace(pacing.paceWpm, pacing)) {}

void RsvpSession::restoreAfterCheckpoint(uint32_t hash, uint16_t length) {
  restoreIdentityPending = true;
  restoreTokenHash = hash;
  restoreTokenLength = length;
}

uint32_t RsvpSession::baseIntervalMs() const { return 60000u / std::max<uint16_t>(1, paceWpm); }
uint16_t RsvpSession::effectiveMaximumWpm() const { return std::min(pacing.maximumWpm, pacing.safeMaximumWpm); }

uint32_t RsvpSession::tokenHash(const char* text, uint16_t length) {
  uint32_t hash = 2166136261U;
  for (uint16_t index = 0; index < length; ++index) {
    hash ^= static_cast<uint8_t>(text[index]);
    hash *= 16777619U;
  }
  return hash;
}

ResumeAnchor RsvpSession::requestedAnchor() const {
  return consumedCount != 0 ? events[consumedCount - 1].anchor : ResumeAnchor{};
}
uint32_t RsvpSession::requestedTokenHash() const {
  if (consumedCount == 0) return 0;
  PreparedWord last;
  const auto& event = events[consumedCount - 1];
  return prepareRsvpWord(event.text, event.textLength, last) ? tokenHash(last.text, last.textLength) : 0;
}
uint16_t RsvpSession::requestedTokenLength() const {
  if (consumedCount == 0) return 0;
  PreparedWord last;
  const auto& event = events[consumedCount - 1];
  return prepareRsvpWord(event.text, event.textLength, last) ? last.textLength : 0;
}

PauseReason RsvpSession::pauseReasonFor(NonTextKind kind) {
  switch (kind) {
    case NonTextKind::Image:
      return PauseReason::Image;
    case NonTextKind::Table:
      return PauseReason::Table;
    case NonTextKind::HorizontalRule:
      return PauseReason::HorizontalRule;
    default:
      return PauseReason::OtherContent;
  }
}

uint16_t RsvpSession::punctuationPausePercent(const char* text, uint16_t length, const RsvpPacingConfig& pacing) {
  uint32_t last = 0;
  for (uint16_t offset = 0; offset < length;) {
    const uint32_t cp = nextCodepoint(text, length, offset);
    if (!isClosingCodepoint(cp)) last = cp;
  }
  if (isSentenceTerminal(last)) return pacing.sentencePausePercent;
  if (last == ',' || last == ';' || last == ':' || last == '-' || last == 0x2013 || last == 0x2014) {
    return pacing.clausePausePercent;
  }
  return 100;
}

bool RsvpSession::boundaryBefore(const DocumentEvent& event) {
  if (!prepareRsvpWord(event.text, event.textLength, boundaryScratch)) return true;
  for (uint16_t offset = 0; offset < boundaryScratch.core.begin;) {
    if (isRsvpGroupingBoundary(nextCodepoint(boundaryScratch.text, boundaryScratch.textLength, offset))) return true;
  }
  return false;
}

bool RsvpSession::boundaryAfter(const DocumentEvent& event) {
  if (!prepareRsvpWord(event.text, event.textLength, boundaryScratch)) return true;
  for (uint16_t offset = boundaryScratch.core.end; offset < boundaryScratch.textLength;) {
    if (isRsvpGroupingBoundary(nextCodepoint(boundaryScratch.text, boundaryScratch.textLength, offset))) return true;
  }
  return false;
}

void RsvpSession::fillDecision(Decision& decision) const {
  decision.state = state;
  decision.pauseReason = PauseReason::None;
  if (state == State::Boundary) decision.pauseReason = fallbackReason;
  if (state == State::Error) decision.pauseReason = PauseReason::Error;
  if (state == State::Paused && chapterPauseShown) decision.pauseReason = PauseReason::Chapter;
  decision.nextDeadlineMs = nextDeadlineMs;
  decision.paceWpm = paceWpm;
  decision.checkpointRequested = decision.checkpointRequested || checkpointRequestedThisStep;
  if ((state == State::Paused || state == State::Playing) && !chapterPauseShown && preparedWord.valid &&
      presentationGroup.count != 0) {
    decision.frame.presentationGroup = &presentationGroup;
  }
  if (state == State::Error) {
    decision.switchToPaged = true;
    decision.pagedModeAvailable = true;
  }
}

void RsvpSession::setError(Decision& decision, Error error) {
  state = State::Error;
  fallbackReason = PauseReason::Error;
  framePresented = false;
  nextDeadlineMs = 0;
  decision = {};
  decision.error = error;
  decision.pagedModeAvailable = true;
  checkpointRequestedThisStep = true;
  fillDecision(decision);
}

bool RsvpSession::bufferThrough(uint8_t index) {
  if (index >= EVENT_CAPACITY) return false;
  while (eventCount <= index) {
    auto& event = events[eventCount++];
    event = {};
    if (!source.next(event)) event.kind = EventKind::Error;
    if (event.textLength > MAX_TOKEN_BYTES) {
      event.kind = EventKind::OversizedWord;
      event.textLength = 0;
    }
    event.text[event.textLength] = '\0';
  }
  return true;
}

void RsvpSession::discardEvents(uint8_t index, uint8_t count) {
  if (count == 0 || index + count > eventCount) return;
  for (uint8_t next = index + count; next < eventCount; ++next) events[next - count] = events[next];
  eventCount = static_cast<uint8_t>(eventCount - count);
}

bool RsvpSession::findNextWord(Decision& decision) {
  while (bufferThrough(0)) {
    switch (events[0].kind) {
      case EventKind::Word:
        return true;
      case EventKind::NonLexicalText:
      case EventKind::ParagraphBoundary:
      case EventKind::ChapterBoundary:
        discardEvents(0, 1);
        break;
      case EventKind::Error:
        setError(decision, Error::SourceRead);
        return false;
      case EventKind::EndOfBook:
        state = State::Finished;
        nextDeadlineMs = 0;
        decision = {};
        fillDecision(decision);
        return false;
      case EventKind::OversizedWord:
      case EventKind::NonText:
        state = State::Boundary;
        fallbackReason =
            events[0].kind == EventKind::OversizedWord ? PauseReason::OversizedWord : pauseReasonFor(events[0].nonText);
        decision = {};
        decision.pagedModeAvailable = true;
        if (pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
          decision.cleanupRefresh = true;
          framesSinceCleanup = 0;
        }
        fillDecision(decision);
        return false;
    }
  }
  setError(decision, Error::SourceRead);
  return false;
}

void RsvpSession::buildGroupView(uint8_t count, uint8_t activeIndex) {
  presentationGroup = {};
  presentationGroup.count = count;
  presentationGroup.activeIndex = activeIndex;
  for (uint8_t index = 0; index < count; ++index) {
    presentationGroup.tokens[index] = {events[index].text, events[index].textLength, events[index].anchor};
  }
}

void RsvpSession::configureCurrentGap() {
  paragraphPending = false;
  chapterPending = false;
  framePausePercent =
      punctuationPausePercent(events[consumedCount - 1].text, events[consumedCount - 1].textLength, pacing);
  // Drain only separators after the group, keeping one unconsumed next event.
  // Group words never move while their view is published.
  while (bufferThrough(consumedCount)) {
    const auto& next = events[consumedCount];
    if (next.kind == EventKind::NonLexicalText) {
      framePausePercent = std::max(framePausePercent, punctuationPausePercent(next.text, next.textLength, pacing));
    } else if (next.kind == EventKind::ParagraphBoundary) {
      paragraphPending = true;
      framePausePercent = std::max(framePausePercent, pacing.paragraphPausePercent);
    } else if (next.kind == EventKind::ChapterBoundary) {
      chapterPending = true;
    } else {
      break;
    }
    discardEvents(consumedCount, 1);
  }
}

bool RsvpSession::prepareGroup(uint8_t count, uint8_t activeIndex, uint32_t nowMs, Decision& decision,
                               bool recordHistory) {
  if (!prepareRsvpWord(events[activeIndex].text, events[activeIndex].textLength, preparedWord)) {
    if (!preparedWord.overflowed) {
      setError(decision, Error::InvalidDocument);
    } else {
      state = State::Boundary;
      fallbackReason = PauseReason::OversizedWord;
      decision = {};
      decision.pagedModeAvailable = true;
      fillDecision(decision);
    }
    return false;
  }
  consumedCount = count;
  buildGroupView(count, activeIndex);
  if (recordHistory) {
    if (historyCount == HISTORY_CAPACITY) {
      for (uint8_t index = 1; index < HISTORY_CAPACITY; ++index) history[index - 1] = history[index];
      --historyCount;
    }
    history[historyCount++] = {events[0].anchor, count, activeIndex};
    historyCursor = historyCount - 1;
  }
  configureCurrentGap();
  currentPauseMs = baseIntervalMs() * (framePausePercent + 35u * (count - 1)) / 100u;
  chapterPauseShown = false;
  state = state == State::Playing ? State::Playing : State::Paused;
  fallbackReason = PauseReason::None;
  framePresented = true;
  nextDeadlineMs = 0;
  decision = {};
  decision.render = true;
  decision.frame.id = ++frameId;
  decision.frame.requestedAtMs = nowMs;
  decision.frame.text = preparedWord.text;
  decision.frame.textLength = preparedWord.textLength;
  decision.frame.anchor = requestedAnchor();
  decision.frame.preparedWord = &preparedWord;
  decision.frame.presentationGroup = &presentationGroup;
  ++framesSinceCleanup;
  if (paragraphPending && pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
    decision.cleanupRefresh = true;
    framesSinceCleanup = 0;
  }
  fillDecision(decision);
  return true;
}

bool RsvpSession::emitNextWord(uint32_t nowMs, Decision& decision) {
  discardEvents(0, consumedCount);
  consumedCount = 0;
  presentationGroup = {};
  if (!findNextWord(decision)) return false;
  if (historyCount != 0 && historyCursor + 1 < historyCount) {
    const auto& entry = history[++historyCursor];
    for (uint8_t index = 0; index < entry.count; ++index) {
      if (!bufferThrough(index) || events[index].kind != EventKind::Word) {
        setError(decision, Error::SourceRead);
        return false;
      }
    }
    return prepareGroup(entry.count, entry.activeIndex, nowMs, decision, false);
  }
  if (!groupingEnabled) return prepareGroup(1, 0, nowMs, decision, true);

  uint8_t activeIndex = 0;
  auto role = classifyRsvpCompanion(events[0].text, events[0].textLength);
  if (role == CompanionRole::Backward) return prepareGroup(1, 0, nowMs, decision, true);
  // A forward chain needs a lexical anchor inside the three-word budget.
  while (role == CompanionRole::Forward || role == CompanionRole::Bidirectional) {
    if (activeIndex == 2 || boundaryAfter(events[activeIndex]) || !bufferThrough(activeIndex + 1) ||
        events[activeIndex + 1].kind != EventKind::Word || boundaryBefore(events[activeIndex + 1])) {
      return prepareGroup(1, 0, nowMs, decision, true);
    }
    ++activeIndex;
    role = classifyRsvpCompanion(events[activeIndex].text, events[activeIndex].textLength);
    if (role == CompanionRole::Backward) return prepareGroup(1, 0, nowMs, decision, true);
  }

  uint8_t count = activeIndex + 1;
  while (count < EVENT_CAPACITY && !boundaryAfter(events[count - 1]) && bufferThrough(count) &&
         events[count].kind == EventKind::Word && !boundaryBefore(events[count])) {
    role = classifyRsvpCompanion(events[count].text, events[count].textLength);
    if (role != CompanionRole::Backward && role != CompanionRole::Bidirectional) break;
    if (count == 3) {
      // A postfix outranks the farthest prefix. Emit that prefix first.
      if (activeIndex != 0) return prepareGroup(1, 0, nowMs, decision, true);
      break;
    }
    ++count;
  }
  if (!prepareRsvpWord(events[activeIndex].text, events[activeIndex].textLength, preparedWord)) {
    return prepareGroup(1, 0, nowMs, decision, true);
  }
  buildGroupView(count, activeIndex);
  if (fitCallback && count > 1) {
    const auto range = fitCallback(fitContext, preparedWord, presentationGroup);
    if (range.begin > activeIndex || range.end <= activeIndex || range.end > count) {
      setError(decision, Error::InvalidDocument);
      return false;
    }
    if (range.begin != 0) return prepareGroup(1, 0, nowMs, decision, true);
    count = range.end;
  }
  return prepareGroup(count, activeIndex, nowMs, decision, true);
}

bool RsvpSession::emitHistoryWord(uint8_t index, uint32_t nowMs, Decision& decision) {
  if (index >= historyCount) return false;
  const auto& entry = history[index];
  if (!source.open(&entry.firstAnchor)) {
    setError(decision, Error::SourceOpen);
    return false;
  }
  eventCount = 0;
  consumedCount = 0;
  presentationGroup = {};
  if (!findNextWord(decision)) return false;
  for (uint8_t word = 0; word < entry.count; ++word) {
    if (!bufferThrough(word) || events[word].kind != EventKind::Word) {
      setError(decision, Error::SourceRead);
      return false;
    }
  }
  historyCursor = index;
  return prepareGroup(entry.count, entry.activeIndex, nowMs, decision, false);
}

Decision RsvpSession::step(const Input& input) {
  checkpointRequestedThisStep = false;
  if (clockInitialized && state == State::Playing) {
    accumulatedActiveMs += static_cast<uint32_t>(input.nowMs - lastObservedNowMs);
  }
  lastObservedNowMs = input.nowMs;
  clockInitialized = true;
  if (state == State::Playing && checkpointClockStarted &&
      static_cast<uint32_t>(input.nowMs - lastCheckpointRequestMs) >= 30000u) {
    if (framePresented) {
      periodicCheckpointPending = true;
    } else {
      checkpointRequestedThisStep = true;
      lastCheckpointRequestMs = input.nowMs;
    }
  }

  Decision decision;
  fillDecision(decision);

  if (state == State::Empty) {
    if (!source.open(initialAnchor.valid ? &initialAnchor : nullptr)) {
      setError(decision, Error::SourceOpen);
      return decision;
    }
    if (restoreIdentityPending) {
      if (!findNextWord(decision) || !prepareRsvpWord(events[0].text, events[0].textLength, boundaryScratch) ||
          tokenHash(boundaryScratch.text, boundaryScratch.textLength) != restoreTokenHash ||
          boundaryScratch.textLength != restoreTokenLength) {
        setError(decision, Error::InvalidDocument);
        return decision;
      }
      restoreIdentityPending = false;
      restoreIdentityValidated = true;
      if (groupingEnabled) {
        presentedAnchor = events[0].anchor;
        presentedTokenHash32 = restoreTokenHash;
        presentedTokenLength = restoreTokenLength;
        discardEvents(0, 1);
      }
    }
    emitNextWord(input.nowMs, decision);
    return decision;
  }

  if (input.action == Action::ModeSwitch || input.action == Action::Exit) {
    checkpointRequestedThisStep = true;
    state = State::Exited;
    nextDeadlineMs = 0;
    decision = {};
    decision.switchToPaged = input.action == Action::ModeSwitch;
    fillDecision(decision);
    return decision;
  }

  if (input.action == Action::FramePresented && frameId != 0 && input.presentedFrameId == frameId) {
    decision.presentationAccepted = true;
    decision.presentedAtMs = input.nowMs;
    decision.refreshDurationMs = input.refreshDurationMs;
    if (framePresented) {
      presentedAnchor = requestedAnchor();
      presentedTokenHash32 = requestedTokenHash();
      presentedTokenLength = requestedTokenLength();
      framePresented = false;
      const uint32_t remainingInterval =
          input.refreshDurationMs < currentPauseMs ? currentPauseMs - input.refreshDurationMs : 0;
      nextDeadlineMs = input.nowMs + remainingInterval;
      if (periodicCheckpointPending) {
        periodicCheckpointPending = false;
        checkpointRequestedThisStep = true;
        lastCheckpointRequestMs = input.nowMs;
      }
      if (checkpointAfterPresentation) {
        checkpointAfterPresentation = false;
        checkpointRequestedThisStep = true;
      }
    }
    fillDecision(decision);
    return decision;
  }

  if (state == State::Error || state == State::Exited || state == State::Finished) return decision;

  switch (input.action) {
    case Action::PaceDown:
      paceWpm = paceWpm > pacing.minimumWpm && paceWpm - pacing.paceStepWpm >= pacing.minimumWpm
                    ? static_cast<uint16_t>(paceWpm - pacing.paceStepWpm)
                    : pacing.minimumWpm;
      break;
    case Action::PaceUp:
      paceWpm = paceWpm < effectiveMaximumWpm() && paceWpm + pacing.paceStepWpm <= effectiveMaximumWpm()
                    ? static_cast<uint16_t>(paceWpm + pacing.paceStepWpm)
                    : effectiveMaximumWpm();
      break;
    case Action::TogglePlayback:
      if (framePresented) break;
      if (state == State::Playing) {
        state = State::Paused;
        nextDeadlineMs = 0;
        checkpointRequestedThisStep = true;
      } else if (state == State::Paused) {
        if (chapterPauseShown) {
          chapterPauseShown = false;
          chapterPending = false;
          state = State::Playing;
          nextDeadlineMs = input.nowMs;
        } else {
          state = State::Playing;
        }
        if (!checkpointClockStarted) {
          checkpointClockStarted = true;
          lastCheckpointRequestMs = input.nowMs;
        }
      }
      break;
    case Action::StepForward: {
      if (state == State::Boundary && isSkippableNonTextBoundary(fallbackReason)) {
        discardEvents(0, 1);
        fallbackReason = PauseReason::None;
        framePresented = false;
        nextDeadlineMs = 0;
        state = State::Paused;
        emitNextWord(input.nowMs, decision);
        checkpointAfterPresentation = decision.render;
      } else if (state == State::Paused && !framePresented) {
        if (chapterPauseShown) chapterPauseShown = false;
        chapterPending = false;
        emitNextWord(input.nowMs, decision);
        checkpointAfterPresentation = decision.render;
      }
      break;
    }
    case Action::RewindFive:
      if (!framePresented && historyCount != 0) {
        const uint8_t target = historyCursor > 5 ? static_cast<uint8_t>(historyCursor - 5) : 0;
        state = State::Paused;
        nextDeadlineMs = 0;
        chapterPending = false;
        chapterPauseShown = false;
        emitHistoryWord(target, input.nowMs, decision);
        checkpointAfterPresentation = decision.render;
      }
      break;
    case Action::WordDoesNotFit:
      state = State::Boundary;
      fallbackReason = PauseReason::OversizedWord;
      nextDeadlineMs = 0;
      framePresented = false;
      decision = {};
      decision.pagedModeAvailable = true;
      fillDecision(decision);
      break;
    case Action::None:
      if (state == State::Playing && !framePresented && input.nowMs >= nextDeadlineMs) {
        if (chapterPending) {
          state = State::Paused;
          checkpointRequestedThisStep = true;
          chapterPauseShown = true;
          nextDeadlineMs = 0;
          decision = {};
          if (pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
            decision.cleanupRefresh = true;
            framesSinceCleanup = 0;
          }
          fillDecision(decision);
        } else {
          emitNextWord(input.nowMs, decision);
        }
      }
      break;
    default:
      break;
  }

  fillDecision(decision);
  return decision;
}

}  // namespace rsvp
