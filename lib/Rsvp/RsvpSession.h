#pragma once

#include "RsvpLexicalCore.h"
#include "RsvpTypes.h"

namespace rsvp {

class RsvpSession final {
 public:
  explicit RsvpSession(RsvpSource& source, ResumeAnchor initialAnchor = {}, RsvpPacingConfig pacing = {});

  Decision step(const Input& input);

 private:
  static constexpr uint8_t HISTORY_CAPACITY = 6;

  bool emitNextWord(uint32_t nowMs, Decision& decision);
  bool fetchNextEvent();
  bool fetchLookahead();
  bool emitHistoryWord(uint8_t historyIndex, uint32_t nowMs, Decision& decision);
  bool emitWord(const DocumentEvent& event, uint32_t nowMs, Decision& decision, bool recordHistory);
  void setError(Decision& decision, Error error);
  void fillDecision(Decision& decision) const;
  uint32_t baseIntervalMs() const;
  uint16_t currentPausePercent() const;
  uint16_t effectiveMaximumWpm() const;
  static PauseReason pauseReasonFor(NonTextKind kind);
  static uint16_t punctuationPausePercent(const char* text, uint16_t length, const RsvpPacingConfig& pacing);

  struct HistoryEntry {
    DocumentEvent event;
  };

  RsvpSource& source;
  ResumeAnchor initialAnchor;
  RsvpPacingConfig pacing;
  DocumentEvent currentEvent;
  PreparedWord preparedWord;
  DocumentEvent lookaheadEvent;
  bool lookaheadValid = false;
  bool paragraphPending = false;
  bool chapterPending = false;
  bool chapterPauseShown = false;
  PauseReason fallbackReason = PauseReason::None;
  uint16_t pendingPunctuationPause = 100;
  State state = State::Empty;
  uint32_t frameId = 0;
  uint32_t nextDeadlineMs = 0;
  uint32_t currentPauseMs = 0;
  uint16_t framePausePercent = 100;
  uint16_t paceWpm = 100;
  bool framePresented = false;
  uint16_t framesSinceCleanup = 0;
  HistoryEntry history[HISTORY_CAPACITY] = {};
  uint8_t historyCount = 0;
  uint8_t historyCursor = 0;
};

}  // namespace rsvp
