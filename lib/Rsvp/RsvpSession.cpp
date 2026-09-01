#include "RsvpSession.h"

#include <algorithm>

namespace rsvp {

namespace {

uint16_t clampPace(const uint16_t pace, const RsvpPacingConfig& config) {
  const uint16_t maximum = std::min(config.maximumWpm, config.safeMaximumWpm);
  return std::min(maximum, std::max(config.minimumWpm, pace));
}

bool isClosingCodepoint(const uint32_t codepoint) {
  return codepoint == ')' || codepoint == ']' || codepoint == '}' || codepoint == '>' || codepoint == 0x00BB ||
         codepoint == 0x2019 || codepoint == 0x201D || codepoint == 0x3009 || codepoint == 0x300B ||
         codepoint == 0x300D || codepoint == 0x300F;
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

RsvpSession::RsvpSession(RsvpSource& source, const ResumeAnchor initialAnchor, const RsvpPacingConfig pacing)
    : source(source), initialAnchor(initialAnchor), pacing(pacing), paceWpm(clampPace(pacing.paceWpm, pacing)) {}

uint32_t RsvpSession::baseIntervalMs() const { return 60000u / std::max<uint16_t>(1, paceWpm); }

uint32_t RsvpSession::currentTokenHash() const {
  uint32_t hash = 2166136261U;
  for (uint16_t index = 0; index < preparedWord.textLength; ++index) {
    hash ^= static_cast<uint8_t>(preparedWord.text[index]);
    hash *= 16777619U;
  }
  return hash;
}

uint16_t RsvpSession::effectiveMaximumWpm() const { return std::min(pacing.maximumWpm, pacing.safeMaximumWpm); }

PauseReason RsvpSession::pauseReasonFor(const NonTextKind kind) {
  switch (kind) {
    case NonTextKind::Image:
      return PauseReason::Image;
    case NonTextKind::Table:
      return PauseReason::Table;
    case NonTextKind::HorizontalRule:
      return PauseReason::HorizontalRule;
    case NonTextKind::Other:
    case NonTextKind::None:
      return PauseReason::OtherContent;
  }
  return PauseReason::OtherContent;
}

uint16_t RsvpSession::punctuationPausePercent(const char* text, const uint16_t length, const RsvpPacingConfig& pacing) {
  uint32_t lastSignificant = 0;
  for (uint16_t offset = 0; offset < length;) {
    const uint32_t codepoint = nextCodepoint(text, length, offset);
    if (!isClosingCodepoint(codepoint)) lastSignificant = codepoint;
  }

  if (lastSignificant == '.' || lastSignificant == '!' || lastSignificant == '?' || lastSignificant == 0x2026) {
    return pacing.sentencePausePercent;
  }
  if (lastSignificant == ',' || lastSignificant == ';' || lastSignificant == ':' || lastSignificant == 0x2014) {
    return pacing.clausePausePercent;
  }
  return 100;
}

uint16_t RsvpSession::currentPausePercent() const { return framePausePercent; }

void RsvpSession::fillDecision(Decision& decision) const {
  decision.state = state;
  decision.pauseReason = PauseReason::None;
  if (state == State::Boundary) decision.pauseReason = fallbackReason;
  if (state == State::Error) decision.pauseReason = PauseReason::Error;
  if (state == State::Paused && chapterPending) decision.pauseReason = PauseReason::Chapter;
  decision.nextDeadlineMs = nextDeadlineMs;
  decision.paceWpm = paceWpm;
  decision.checkpointRequested = decision.checkpointRequested || checkpointRequestedThisStep;
  if (state == State::Error) {
    decision.switchToPaged = true;
    decision.pagedModeAvailable = true;
  }
}

void RsvpSession::setError(Decision& decision, const Error error) {
  state = State::Error;
  fallbackReason = PauseReason::Error;
  framePresented = false;
  nextDeadlineMs = 0;
  decision.error = error;
  decision.pagedModeAvailable = true;
  checkpointRequestedThisStep = true;
  fillDecision(decision);
}

bool RsvpSession::fetchNextEvent() {
  if (lookaheadValid) return true;
  if (!source.next(lookaheadEvent)) {
    lookaheadEvent = {};
    lookaheadEvent.kind = EventKind::Error;
  }
  lookaheadValid = true;
  return true;
}

bool RsvpSession::fetchLookahead() {
  lookaheadValid = false;
  paragraphPending = false;
  chapterPending = false;
  pendingPunctuationPause = 100;
  while (true) {
    fetchNextEvent();
    switch (lookaheadEvent.kind) {
      case EventKind::Word:
        return true;
      case EventKind::NonLexicalText:
        pendingPunctuationPause = std::max(
            pendingPunctuationPause, punctuationPausePercent(lookaheadEvent.text, lookaheadEvent.textLength, pacing));
        lookaheadValid = false;
        continue;
      case EventKind::ParagraphBoundary:
        paragraphPending = true;
        lookaheadValid = false;
        continue;
      case EventKind::ChapterBoundary:
        chapterPending = true;
        lookaheadValid = false;
        continue;
      default:
        return true;
    }
  }
}

bool RsvpSession::emitWord(const DocumentEvent& event, const uint32_t nowMs, Decision& decision,
                           const bool recordHistory) {
  PreparedWord candidateWord;
  if (!prepareRsvpWord(event.text, event.textLength, candidateWord)) {
    state = candidateWord.overflowed ? State::Boundary : State::Error;
    fallbackReason = candidateWord.overflowed ? PauseReason::OversizedWord : PauseReason::Error;
    decision = {};
    decision.error = candidateWord.overflowed ? Error::None : Error::InvalidDocument;
    decision.pagedModeAvailable = true;
    fillDecision(decision);
    return false;
  }
  currentEvent = event;
  preparedWord = candidateWord;
  if (recordHistory) {
    if (historyCursor + 1 < historyCount) historyCount = static_cast<uint8_t>(historyCursor + 1);
    if (historyCount == HISTORY_CAPACITY) {
      for (uint8_t index = 1; index < HISTORY_CAPACITY; index++) history[index - 1] = history[index];
      historyCount--;
      if (historyCursor > 0) historyCursor--;
    }
    history[historyCount++].event = currentEvent;
    historyCursor = static_cast<uint8_t>(historyCount - 1);
  }

  switch (preparedWord.pauseClass) {
    case PauseClass::Clause:
      framePausePercent = pacing.clausePausePercent;
      break;
    case PauseClass::Sentence:
      framePausePercent = pacing.sentencePausePercent;
      break;
    case PauseClass::None:
      framePausePercent = 100;
      break;
  }
  fetchLookahead();
  framePausePercent = std::max(framePausePercent, pendingPunctuationPause);
  if (paragraphPending) framePausePercent = std::max(framePausePercent, pacing.paragraphPausePercent);
  currentPauseMs = baseIntervalMs() * framePausePercent / 100u;
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
  decision.frame.anchor = currentEvent.anchor;
  decision.frame.preparedWord = &preparedWord;
  framesSinceCleanup++;
  if (paragraphPending && pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
    decision.cleanupRefresh = true;
    framesSinceCleanup = 0;
  }
  fillDecision(decision);
  return true;
}

bool RsvpSession::emitNextWord(const uint32_t nowMs, Decision& decision) {
  if (historyCursor + 1 < historyCount) {
    historyCursor++;
    return emitWord(history[historyCursor].event, nowMs, decision, false);
  }
  if (!lookaheadValid) fetchLookahead();
  if (lookaheadEvent.kind == EventKind::Word) {
    const DocumentEvent event = lookaheadEvent;
    lookaheadValid = false;
    return emitWord(event, nowMs, decision, true);
  }
  if (lookaheadEvent.kind == EventKind::NonText || lookaheadEvent.kind == EventKind::OversizedWord) {
    state = State::Boundary;
    fallbackReason = lookaheadEvent.kind == EventKind::OversizedWord ? PauseReason::OversizedWord
                                                                     : pauseReasonFor(lookaheadEvent.nonText);
    decision = {};
    decision.pagedModeAvailable = true;
    if (pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
      decision.cleanupRefresh = true;
      framesSinceCleanup = 0;
    }
    fillDecision(decision);
    return false;
  }
  if (lookaheadEvent.kind == EventKind::Error) {
    setError(decision, Error::SourceRead);
    return false;
  }
  if (lookaheadEvent.kind == EventKind::EndOfBook) {
    state = State::Finished;
    nextDeadlineMs = 0;
    decision = {};
    fillDecision(decision);
    return false;
  }
  return false;
}

bool RsvpSession::emitHistoryWord(const uint8_t historyIndex, const uint32_t nowMs, Decision& decision) {
  if (historyIndex >= historyCount) return false;
  const ResumeAnchor anchor = history[historyIndex].event.anchor;
  if (!source.open(&anchor)) {
    setError(decision, Error::SourceOpen);
    return false;
  }
  DocumentEvent consumed;
  if (!source.next(consumed) || consumed.kind != EventKind::Word) {
    setError(decision, Error::SourceRead);
    return false;
  }
  historyCursor = historyIndex;
  lookaheadValid = false;
  return emitWord(history[historyIndex].event, nowMs, decision, false);
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
    checkpointRequestedThisStep = true;
    lastCheckpointRequestMs = input.nowMs;
  }

  Decision decision;
  fillDecision(decision);

  if (state == State::Empty) {
    if (!source.open(initialAnchor.valid ? &initialAnchor : nullptr)) {
      setError(decision, Error::SourceOpen);
      return decision;
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
      framePresented = false;
      const uint32_t remainingInterval =
          input.refreshDurationMs < currentPauseMs ? currentPauseMs - input.refreshDurationMs : 0;
      nextDeadlineMs = input.nowMs + remainingInterval;
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
    case Action::StepForward:
      if (state == State::Paused && !framePresented) {
        if (chapterPauseShown) chapterPauseShown = false;
        chapterPending = false;
        emitNextWord(input.nowMs, decision);
        checkpointRequestedThisStep = true;
      }
      break;
    case Action::RewindFive:
      if (!framePresented && historyCount != 0) {
        const uint8_t target = historyCursor > 5 ? static_cast<uint8_t>(historyCursor - 5) : 0;
        state = State::Paused;
        nextDeadlineMs = 0;
        chapterPending = false;
        chapterPauseShown = false;
        emitHistoryWord(target, input.nowMs, decision);
        checkpointRequestedThisStep = true;
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
