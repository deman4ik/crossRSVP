#pragma once

#include "RsvpTypes.h"

namespace rsvp {

enum class ReadingMode : uint8_t { Paged, Rsvp };

struct PagedResumeContext {
  ResumeAnchor currentAnchor;
  ResumeAnchor pageStartAnchor;
  bool pageTurned = false;
};

struct ModeSwitchDecision {
  ReadingMode mode = ReadingMode::Paged;
  ResumeAnchor anchor;
  bool temporaryHighlight = false;
  bool paused = false;
};

class RsvpModeSwitch final {
 public:
  static ModeSwitchDecision newBook();
  static ModeSwitchDecision fromRsvp(const ResumeAnchor& lastDisplayedAnchor);
  static ModeSwitchDecision fromPaged(const PagedResumeContext& context);
};

}  // namespace rsvp
