#pragma once

#include "RsvpCompanionPolicy.h"
#include "RsvpLexicalCore.h"
#include "RsvpTypes.h"

namespace rsvp {

class RsvpSession final {
 public:
  explicit RsvpSession(RsvpSource& source, ResumeAnchor initialAnchor = {}, const RsvpPacingConfig& pacing = {},
                       bool groupingEnabled = false, PresentationGroupFitCallback fitCallback = nullptr,
                       void* fitContext = nullptr, GroupingLanguage groupingLanguage = GroupingLanguage::Russian);
  Decision step(const Input& input);
  // Applies settings without rebuilding the source or replacing the displayed group.
  // Only a paused session may be reconfigured; the new grouping policy starts with the next group.
  Decision configureWhilePaused(const RsvpPacingConfig& newPacing, bool newGroupingEnabled);
  Decision configureWhilePaused(const RsvpPacingConfig& newPacing, bool newGroupingEnabled, GroupingLanguage language);
  ResumeAnchor currentAnchor() const { return presentedAnchor; }
  uint32_t currentTokenHash() const { return presentedTokenHash32; }
  uint16_t currentTokenLength() const { return presentedTokenLength; }
  ResumeAnchor requestedAnchor() const;
  uint32_t requestedTokenHash() const;
  uint16_t requestedTokenLength() const;
  uint64_t activeReadingMs() const { return accumulatedActiveMs; }
  void restoreAfterCheckpoint(uint32_t tokenHash, uint16_t tokenLength);
  bool checkpointIdentityValidated() const { return restoreIdentityValidated; }

 private:
  static constexpr uint8_t HISTORY_CAPACITY = 6;
  static constexpr uint8_t EVENT_CAPACITY = 4;
  bool emitNextWord(uint32_t nowMs, Decision& decision);
  bool emitHistoryWord(uint8_t historyIndex, uint32_t nowMs, Decision& decision);
  bool prepareGroup(uint8_t count, uint8_t activeIndex, uint32_t nowMs, Decision& decision, bool recordHistory);
  bool findNextWord(Decision& decision);
  bool bufferThrough(uint8_t index);
  void discardEvents(uint8_t index, uint8_t count);
  void configureCurrentGap();
  void buildGroupView(uint8_t count, uint8_t activeIndex);
  bool boundaryBefore(const DocumentEvent& event);
  bool boundaryAfter(const DocumentEvent& event);
  void setError(Decision& decision, Error error);
  void fillDecision(Decision& decision) const;
  uint32_t baseIntervalMs() const;
  uint16_t effectiveMaximumWpm() const;
  static uint32_t tokenHash(const char* text, uint16_t length);
  static PauseReason pauseReasonFor(NonTextKind kind);
  static uint16_t punctuationPausePercent(const char* text, uint16_t length, const RsvpPacingConfig& pacing);

  // Six replay descriptors replace the old full word-history entries.
  // At most 24 bytes each; token text is re-read from the source.
  struct HistoryEntry {
    ResumeAnchor firstAnchor;
    uint8_t count = 1;
    uint8_t activeIndex = 0;
  };
  static_assert(sizeof(HistoryEntry) <= 24);

  RsvpSource& source;
  ResumeAnchor initialAnchor;
  RsvpPacingConfig pacing;
  bool groupingEnabled = false;
  GroupingLanguage groupingLanguage = GroupingLanguage::Russian;
  PresentationGroupFitCallback fitCallback = nullptr;
  void* fitContext = nullptr;
  // Three source words plus one lookahead. The displayed group stays in the
  // queue prefix until advance; there is no parallel token-history buffer.
  DocumentEvent events[EVENT_CAPACITY] = {};
  uint8_t eventCount = 0;
  uint8_t consumedCount = 0;
  PreparedWord preparedWord;
  PreparedWord boundaryScratch;
  PresentationGroup presentationGroup;
  bool restoreIdentityPending = false;
  bool restoreIdentityValidated = false;
  uint32_t restoreTokenHash = 0;
  uint16_t restoreTokenLength = 0;
  bool paragraphPending = false;
  bool chapterPending = false;
  bool chapterPauseShown = false;
  PauseReason fallbackReason = PauseReason::None;
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
