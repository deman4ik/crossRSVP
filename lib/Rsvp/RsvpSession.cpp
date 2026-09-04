#include "RsvpSession.h"

#include <algorithm>
#include <cstring>

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

bool endsSentence(const char* text, const uint16_t length) {
  uint32_t lastSignificant = 0;
  for (uint16_t offset = 0; offset < length;) {
    const uint32_t codepoint = nextCodepoint(text, length, offset);
    if (!isClosingCodepoint(codepoint)) lastSignificant = codepoint;
  }
  return isSentenceTerminal(lastSignificant);
}

bool wordEndsSentence(const char* text, const uint16_t length) {
  PreparedWord word;
  return prepareRsvpWord(text, length, word) && word.pauseClass == PauseClass::Sentence;
}

}  // namespace

RsvpSession::RsvpSession(RsvpSource& source, const ResumeAnchor initialAnchor, const RsvpPacingConfig pacing,
                         const bool contextLineEnabled)
    : source(source),
      initialAnchor(initialAnchor),
      pacing(pacing),
      contextLineEnabled(contextLineEnabled),
      paceWpm(clampPace(pacing.paceWpm, pacing)) {}

uint32_t RsvpSession::baseIntervalMs() const { return 60000u / std::max<uint16_t>(1, paceWpm); }

uint32_t RsvpSession::tokenHash(const PreparedWord& word) {
  uint32_t hash = 2166136261U;
  for (uint16_t index = 0; index < word.textLength; ++index) {
    hash ^= static_cast<uint8_t>(word.text[index]);
    hash *= 16777619U;
  }
  return hash;
}

uint32_t RsvpSession::currentTokenHash() const { return presentedTokenHash32; }

uint32_t RsvpSession::requestedTokenHash() const { return tokenHash(preparedWord); }

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

  if (isSentenceTerminal(lastSignificant)) {
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
  if (contextLineEnabled && (state == State::Paused || state == State::Playing) &&
      !(state == State::Paused && chapterPending) && preparedWord.valid && contextWindow.count != 0) {
    decision.frame.contextWindow = &contextWindow;
  }
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

void RsvpSession::clearReadAhead() {
  readAheadCount = 0;
  readAheadTextUsed = 0;
  deferredEventValid = false;
}

bool RsvpSession::bufferNextEvent() {
  if (readAheadCount >= READ_AHEAD_EVENT_CAPACITY) return false;

  DocumentEvent candidate;
  if (deferredEventValid) {
    candidate = deferredEvent;
  } else if (!source.next(candidate)) {
    candidate = {};
    candidate.kind = EventKind::Error;
  }

  const uint16_t storageLength = candidate.textLength == 0 ? 0 : static_cast<uint16_t>(candidate.textLength + 1);
  if (readAheadTextUsed + storageLength > READ_AHEAD_TEXT_CAPACITY) {
    deferredEvent = candidate;
    deferredEventValid = true;
    return false;
  }

  auto& buffered = readAhead[readAheadCount++];
  buffered.kind = candidate.kind;
  buffered.anchor = candidate.anchor;
  buffered.nonText = candidate.nonText;
  buffered.textOffset = readAheadTextUsed;
  buffered.textLength = candidate.textLength;
  if (candidate.textLength != 0) {
    memcpy(readAheadText + readAheadTextUsed, candidate.text, candidate.textLength);
    readAheadText[readAheadTextUsed + candidate.textLength] = '\0';
    readAheadTextUsed = static_cast<uint16_t>(readAheadTextUsed + storageLength);
  }
  deferredEventValid = false;
  return true;
}

bool RsvpSession::peekReadAhead(const uint8_t index, DocumentEvent& event) const {
  if (index >= readAheadCount) return false;
  const auto& buffered = readAhead[index];
  event = {};
  event.kind = buffered.kind;
  event.anchor = buffered.anchor;
  event.nonText = buffered.nonText;
  event.textLength = buffered.textLength;
  if (buffered.textLength != 0) {
    memcpy(event.text, readAheadText + buffered.textOffset, buffered.textLength);
    event.text[buffered.textLength] = '\0';
  }
  return true;
}

bool RsvpSession::popReadAhead(DocumentEvent& event) {
  if (!peekReadAhead(0, event)) return false;
  const auto first = readAhead[0];
  if (first.textLength != 0) {
    const uint16_t removed = static_cast<uint16_t>(first.textLength + 1);
    const uint16_t tailOffset = static_cast<uint16_t>(first.textOffset + removed);
    memmove(readAheadText + first.textOffset, readAheadText + tailOffset, readAheadTextUsed - tailOffset);
    readAheadTextUsed = static_cast<uint16_t>(readAheadTextUsed - removed);
    for (uint8_t index = 1; index < readAheadCount; ++index) {
      if (readAhead[index].textOffset >= tailOffset) {
        readAhead[index].textOffset = static_cast<uint16_t>(readAhead[index].textOffset - removed);
      }
    }
  }
  for (uint8_t index = 1; index < readAheadCount; ++index) readAhead[index - 1] = readAhead[index];
  --readAheadCount;
  return true;
}

bool RsvpSession::readAheadShouldStop(const bool includeVisualContext) const {
  bool logicalTargetFound = false;
  bool contextBoundary = preparedWord.valid && wordEndsSentence(preparedWord.text, preparedWord.textLength);
  uint8_t visualTokens = 0;
  for (uint8_t index = 0; index < readAheadCount; ++index) {
    const auto& event = readAhead[index];
    const char* text = event.textLength == 0 ? nullptr : readAheadText + event.textOffset;
    switch (event.kind) {
      case EventKind::Word: {
        logicalTargetFound = true;
        if (includeVisualContext && !contextBoundary) {
          ++visualTokens;
          if (wordEndsSentence(text, event.textLength)) contextBoundary = true;
        }
        break;
      }
      case EventKind::NonLexicalText:
        if (endsSentence(text, event.textLength)) {
          contextBoundary = true;
        } else if (includeVisualContext && !contextBoundary) {
          ++visualTokens;
        }
        break;
      case EventKind::ParagraphBoundary:
      case EventKind::ChapterBoundary:
        contextBoundary = true;
        break;
      case EventKind::NonText:
      case EventKind::OversizedWord:
      case EventKind::EndOfBook:
      case EventKind::Error:
        return true;
    }
    if (logicalTargetFound &&
        (!includeVisualContext || !contextLineEnabled || contextBoundary || visualTokens >= CONTEXT_SIDE_CAPACITY)) {
      return true;
    }
  }
  return false;
}

void RsvpSession::ensureReadAhead(const bool includeVisualContext) {
  while (!readAheadShouldStop(includeVisualContext)) {
    if (!bufferNextEvent()) break;
  }
}

bool RsvpSession::takeNextWord(DocumentEvent& event, Decision& decision) {
  while (true) {
    ensureReadAhead(false);
    if (!peekReadAhead(0, event)) {
      setError(decision, Error::SourceRead);
      return false;
    }
    switch (event.kind) {
      case EventKind::Word:
        popReadAhead(event);
        return true;
      case EventKind::NonLexicalText:
      case EventKind::ParagraphBoundary:
      case EventKind::ChapterBoundary:
        popReadAhead(event);
        continue;
      case EventKind::NonText:
      case EventKind::OversizedWord:
        state = State::Boundary;
        fallbackReason =
            event.kind == EventKind::OversizedWord ? PauseReason::OversizedWord : pauseReasonFor(event.nonText);
        decision = {};
        decision.pagedModeAvailable = true;
        fillDecision(decision);
        return false;
      case EventKind::Error:
        setError(decision, Error::SourceRead);
        return false;
      case EventKind::EndOfBook:
        state = State::Finished;
        nextDeadlineMs = 0;
        decision = {};
        fillDecision(decision);
        return false;
    }
  }
}

void RsvpSession::configureCurrentGap() {
  paragraphPending = false;
  chapterPending = false;
  pendingPunctuationPause = 100;
  if (historyCount != 0 && historyCursor < historyCount) {
    auto& entry = history[historyCursor];
    entry.trailingText[0] = '\0';
    entry.trailingLength = 0;
    entry.contextBoundaryAfter = wordEndsSentence(preparedWord.text, preparedWord.textLength);
  }

  for (uint8_t index = 0; index < readAheadCount; ++index) {
    const auto& event = readAhead[index];
    const char* text = event.textLength == 0 ? nullptr : readAheadText + event.textOffset;
    if (event.kind == EventKind::Word) break;
    if (event.kind == EventKind::NonLexicalText) {
      const uint16_t pause = punctuationPausePercent(text, event.textLength, pacing);
      pendingPunctuationPause = std::max(pendingPunctuationPause, pause);
      const bool sentenceBoundary = endsSentence(text, event.textLength);
      if (historyCount != 0 && historyCursor < historyCount) {
        auto& entry = history[historyCursor];
        if (sentenceBoundary) {
          entry.contextBoundaryAfter = true;
        } else if (!entry.contextBoundaryAfter && entry.trailingLength == 0 &&
                   event.textLength <= CONTEXT_SEPARATOR_CAPACITY) {
          memcpy(entry.trailingText, text, event.textLength);
          entry.trailingText[event.textLength] = '\0';
          entry.trailingLength = event.textLength;
        }
      }
      continue;
    }
    if (event.kind == EventKind::ParagraphBoundary) {
      paragraphPending = true;
      if (historyCount != 0 && historyCursor < historyCount) history[historyCursor].contextBoundaryAfter = true;
      continue;
    } else if (event.kind == EventKind::ChapterBoundary) {
      chapterPending = true;
      if (historyCount != 0 && historyCursor < historyCount) history[historyCursor].contextBoundaryAfter = true;
      continue;
    }
    if (historyCount != 0 && historyCursor < historyCount) history[historyCursor].contextBoundaryAfter = true;
    break;
  }
}

void RsvpSession::buildContextWindow() {
  contextWindow = {};
  if (!contextLineEnabled || !preparedWord.valid || historyCount == 0 || historyCursor >= historyCount) return;

  ContextWindowToken previousNearest[CONTEXT_SIDE_CAPACITY] = {};
  uint8_t previousCount = 0;
  for (int index = static_cast<int>(historyCursor) - 1; index >= 0 && previousCount < CONTEXT_SIDE_CAPACITY; --index) {
    const auto& entry = history[index];
    if (entry.contextBoundaryAfter) break;
    if (entry.trailingLength != 0 && previousCount < CONTEXT_SIDE_CAPACITY) {
      previousNearest[previousCount++] = {entry.trailingText, entry.trailingLength, true};
    }
    if (previousCount < CONTEXT_SIDE_CAPACITY) {
      previousNearest[previousCount++] = {entry.event.text, entry.event.textLength, false};
    }
  }
  while (previousCount != 0) contextWindow.tokens[contextWindow.count++] = previousNearest[--previousCount];

  contextWindow.activeIndex = contextWindow.count;
  contextWindow.tokens[contextWindow.count++] = {preparedWord.text, preparedWord.textLength, false};
  if (wordEndsSentence(preparedWord.text, preparedWord.textLength)) return;

  uint8_t followingCount = 0;
  for (uint8_t index = 0; index < readAheadCount && followingCount < CONTEXT_SIDE_CAPACITY; ++index) {
    const auto& buffered = readAhead[index];
    if (buffered.kind == EventKind::ParagraphBoundary || buffered.kind == EventKind::ChapterBoundary ||
        buffered.kind == EventKind::NonText || buffered.kind == EventKind::OversizedWord ||
        buffered.kind == EventKind::EndOfBook || buffered.kind == EventKind::Error) {
      break;
    }
    const char* text = buffered.textLength == 0 ? nullptr : readAheadText + buffered.textOffset;
    if (buffered.kind == EventKind::NonLexicalText) {
      if (endsSentence(text, buffered.textLength)) break;
      contextWindow.tokens[contextWindow.count++] = {text, buffered.textLength, true};
      ++followingCount;
      continue;
    }
    if (buffered.kind == EventKind::Word) {
      contextWindow.tokens[contextWindow.count++] = {text, buffered.textLength, false};
      ++followingCount;
      if (wordEndsSentence(text, buffered.textLength)) break;
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
    history[historyCount] = {};
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
  ensureReadAhead(true);
  configureCurrentGap();
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
  buildContextWindow();
  decision.frame.contextWindow = contextLineEnabled ? &contextWindow : nullptr;
  framesSinceCleanup++;
  if (paragraphPending && pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
    decision.cleanupRefresh = true;
    framesSinceCleanup = 0;
  }
  fillDecision(decision);
  return true;
}

bool RsvpSession::emitNextWord(const uint32_t nowMs, Decision& decision) {
  DocumentEvent nextEvent;
  if (historyCursor + 1 < historyCount) {
    if (!takeNextWord(nextEvent, decision)) return false;
    ++historyCursor;
    return emitWord(nextEvent, nowMs, decision, false);
  }
  ensureReadAhead(false);
  if (!peekReadAhead(0, nextEvent)) {
    setError(decision, Error::SourceRead);
    return false;
  }
  if (nextEvent.kind == EventKind::Word || nextEvent.kind == EventKind::NonLexicalText ||
      nextEvent.kind == EventKind::ParagraphBoundary || nextEvent.kind == EventKind::ChapterBoundary) {
    if (!takeNextWord(nextEvent, decision)) return false;
    return emitWord(nextEvent, nowMs, decision, true);
  }
  if (nextEvent.kind == EventKind::NonText || nextEvent.kind == EventKind::OversizedWord) {
    state = State::Boundary;
    fallbackReason =
        nextEvent.kind == EventKind::OversizedWord ? PauseReason::OversizedWord : pauseReasonFor(nextEvent.nonText);
    decision = {};
    decision.pagedModeAvailable = true;
    if (pacing.cleanupEveryFrames != 0 && framesSinceCleanup >= pacing.cleanupEveryFrames) {
      decision.cleanupRefresh = true;
      framesSinceCleanup = 0;
    }
    fillDecision(decision);
    return false;
  }
  if (nextEvent.kind == EventKind::Error) {
    setError(decision, Error::SourceRead);
    return false;
  }
  if (nextEvent.kind == EventKind::EndOfBook) {
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
  clearReadAhead();
  DocumentEvent consumed;
  if (!takeNextWord(consumed, decision) || consumed.kind != EventKind::Word) {
    setError(decision, Error::SourceRead);
    return false;
  }
  historyCursor = historyIndex;
  return emitWord(consumed, nowMs, decision, false);
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
      presentedAnchor = currentEvent.anchor;
      presentedTokenHash32 = tokenHash(preparedWord);
      presentedTokenLength = preparedWord.textLength;
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
        DocumentEvent skipped;
        popReadAhead(skipped);
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
