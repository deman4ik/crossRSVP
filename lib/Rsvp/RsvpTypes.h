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
};

enum class State : uint8_t { Empty, Paused, Playing, Boundary, Error, Finished, Exited };

enum class Error : uint8_t { None, SourceOpen, SourceRead, InvalidDocument };

struct Input {
  uint32_t nowMs = 0;
  Action action = Action::None;
  uint32_t presentedFrameId = 0;
  uint32_t refreshDurationMs = 0;
};

struct Frame {
  uint32_t id = 0;
  uint32_t requestedAtMs = 0;
  const char* text = nullptr;
  uint16_t textLength = 0;
  ResumeAnchor anchor;
};

struct Decision {
  State state = State::Empty;
  Error error = Error::None;
  bool render = false;
  bool switchToPaged = false;
  bool presentationAccepted = false;
  uint32_t presentedAtMs = 0;
  uint32_t refreshDurationMs = 0;
  Frame frame;
};

}  // namespace rsvp
