#pragma once

#include <cstddef>
#include <cstdint>

namespace rsvp {

constexpr size_t MAX_TOKEN_BYTES = 200;

struct ResumeAnchor {
  uint16_t spineIndex = 0;
  uint32_t visibleTextOffset = 0;
  uint16_t sameOffsetOrdinal = 0;
  bool valid = false;
};

enum class EventKind : uint8_t {
  Word,
  NonLexicalText,
  ParagraphBoundary,
  ChapterBoundary,
  NonText,
  OversizedWord,
  EndOfBook,
  Error,
};

enum class NonTextKind : uint8_t { None, Image, Table, HorizontalRule, Other };

struct DocumentEvent {
  EventKind kind = EventKind::EndOfBook;
  ResumeAnchor anchor;
  NonTextKind nonText = NonTextKind::None;
  uint16_t textLength = 0;
  char text[MAX_TOKEN_BYTES + 1] = {};
};

class RsvpSource {
 public:
  virtual ~RsvpSource() = default;
  virtual bool open(const ResumeAnchor* anchor = nullptr) = 0;
  virtual bool next(DocumentEvent& out) = 0;
};

enum class Action : uint8_t {
  None,
  TogglePlayback,
  StepForward,
  RewindFive,
  PaceDown,
  PaceUp,
  ModeSwitch,
  Exit,
  FramePresented,
  WordDoesNotFit,
};

enum class State : uint8_t { Empty, Paused, Playing, Boundary, Error, Finished, Exited };

enum class Error : uint8_t { None, SourceOpen, SourceRead, InvalidDocument };

enum class PauseReason : uint8_t {
  None,
  Chapter,
  Image,
  Table,
  HorizontalRule,
  OtherContent,
  OversizedWord,
  Error,
};

struct RsvpPacingConfig {
  uint16_t paceWpm = 100;
  uint16_t minimumWpm = 60;
  uint16_t maximumWpm = 120;
  uint16_t safeMaximumWpm = 100;
  uint16_t paceStepWpm = 10;
  uint16_t clausePausePercent = 150;
  uint16_t sentencePausePercent = 200;
  uint16_t paragraphPausePercent = 250;
  uint16_t cleanupEveryFrames = 20;
};

struct Input {
  uint32_t nowMs = 0;
  Action action = Action::None;
  uint32_t presentedFrameId = 0;
  uint32_t refreshDurationMs = 0;
};

struct PreparedWord;

struct Frame {
  uint32_t id = 0;
  uint32_t requestedAtMs = 0;
  const char* text = nullptr;
  uint16_t textLength = 0;
  ResumeAnchor anchor;
  const PreparedWord* preparedWord = nullptr;
};

struct Decision {
  State state = State::Empty;
  Error error = Error::None;
  PauseReason pauseReason = PauseReason::None;
  bool render = false;
  bool switchToPaged = false;
  bool pagedModeAvailable = false;
  bool cleanupRefresh = false;
  bool presentationAccepted = false;
  bool checkpointRequested = false;
  uint32_t presentedAtMs = 0;
  uint32_t refreshDurationMs = 0;
  uint32_t nextDeadlineMs = 0;
  uint16_t paceWpm = 100;
  Frame frame;
};

}  // namespace rsvp
