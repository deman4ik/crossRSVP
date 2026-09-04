#pragma once

#include "RsvpLexicalCore.h"
#include "RsvpTypes.h"

namespace rsvp {

class RsvpSession final {
 public:
  explicit RsvpSession(RsvpSource& source, ResumeAnchor initialAnchor = {}, RsvpPacingConfig pacing = {},
                       bool contextLineEnabled = false);

  Decision step(const Input& input);
  ResumeAnchor currentAnchor() const { return presentedAnchor; }
  uint32_t currentTokenHash() const;
  uint16_t currentTokenLength() const { return presentedTokenLength; }
  ResumeAnchor requestedAnchor() const { return currentEvent.anchor; }
  uint32_t requestedTokenHash() const;
  uint16_t requestedTokenLength() const { return preparedWord.textLength; }
  uint64_t activeReadingMs() const { return accumulatedActiveMs; }

 private:
  static constexpr uint8_t HISTORY_CAPACITY = 6;
  static constexpr uint8_t CONTEXT_SEPARATOR_CAPACITY = 48;
  static constexpr uint8_t READ_AHEAD_EVENT_CAPACITY = 8;
  static constexpr uint16_t READ_AHEAD_TEXT_CAPACITY = 512;

  bool emitNextWord(uint32_t nowMs, Decision& decision);
  bool bufferNextEvent();
  void ensureReadAhead(bool includeVisualContext);
  bool readAheadShouldStop(bool includeVisualContext) const;
  bool peekReadAhead(uint8_t index, DocumentEvent& event) const;
  bool popReadAhead(DocumentEvent& event);
  void clearReadAhead();
  bool emitHistoryWord(uint8_t historyIndex, uint32_t nowMs, Decision& decision);
  bool emitWord(const DocumentEvent& event, uint32_t nowMs, Decision& decision, bool recordHistory);
  bool takeNextWord(DocumentEvent& event, Decision& decision);
  void configureCurrentGap();
  void buildContextWindow();
  void setError(Decision& decision, Error error);
  void fillDecision(Decision& decision) const;
  uint32_t baseIntervalMs() const;
  static uint32_t tokenHash(const PreparedWord& word);
  uint16_t currentPausePercent() const;
  uint16_t effectiveMaximumWpm() const;
  static PauseReason pauseReasonFor(NonTextKind kind);
  static uint16_t punctuationPausePercent(const char* text, uint16_t length, const RsvpPacingConfig& pacing);

  struct HistoryEntry {
    DocumentEvent event;
    char trailingText[CONTEXT_SEPARATOR_CAPACITY + 1] = {};
    uint16_t trailingLength = 0;
    bool contextBoundaryAfter = false;
  };

  struct BufferedEvent {
    EventKind kind = EventKind::EndOfBook;
    ResumeAnchor anchor;
    NonTextKind nonText = NonTextKind::None;
    uint16_t textOffset = 0;
    uint16_t textLength = 0;
  };

  RsvpSource& source;
  ResumeAnchor initialAnchor;
  RsvpPacingConfig pacing;
  bool contextLineEnabled = false;
  DocumentEvent currentEvent;
  PreparedWord preparedWord;
  BufferedEvent readAhead[READ_AHEAD_EVENT_CAPACITY] = {};
  char readAheadText[READ_AHEAD_TEXT_CAPACITY] = {};
  uint16_t readAheadTextUsed = 0;
  uint8_t readAheadCount = 0;
  DocumentEvent deferredEvent;
  bool deferredEventValid = false;
  ContextWindow contextWindow;
  bool paragraphPending = false;
  bool chapterPending = false;
  bool chapterPauseShown = false;
  PauseReason fallbackReason = PauseReason::None;
  uint16_t pendingPunctuationPause = 100;
  State state = State::Empty;
  uint32_t frameId = 0;
  ResumeAnchor presentedAnchor;
  uint32_t presentedTokenHash32 = 0;
  uint16_t presentedTokenLength = 0;
  uint32_t nextDeadlineMs = 0;
  uint32_t currentPauseMs = 0;
  uint16_t framePausePercent = 100;
  uint16_t paceWpm = 100;
  bool framePresented = false;
  bool checkpointRequestedThisStep = false;
  bool periodicCheckpointPending = false;
  bool checkpointAfterPresentation = false;
  bool checkpointClockStarted = false;
  bool clockInitialized = false;
  uint32_t lastCheckpointRequestMs = 0;
  uint32_t lastObservedNowMs = 0;
  uint64_t accumulatedActiveMs = 0;
  uint16_t framesSinceCleanup = 0;
  HistoryEntry history[HISTORY_CAPACITY] = {};
  uint8_t historyCount = 0;
  uint8_t historyCursor = 0;
};

}  // namespace rsvp
