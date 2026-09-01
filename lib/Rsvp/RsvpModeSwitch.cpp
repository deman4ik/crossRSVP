#include "RsvpModeSwitch.h"

namespace rsvp {

ModeSwitchDecision RsvpModeSwitch::newBook() { return {}; }

ModeSwitchDecision RsvpModeSwitch::fromRsvp(const ResumeAnchor& lastDisplayedAnchor) {
  ModeSwitchDecision decision;
  decision.mode = ReadingMode::Paged;
  decision.anchor = lastDisplayedAnchor;
  decision.temporaryHighlight = true;
  return decision;
}

ModeSwitchDecision RsvpModeSwitch::fromPaged(const PagedResumeContext& context) {
  ModeSwitchDecision decision;
  decision.mode = ReadingMode::Rsvp;
  decision.anchor = context.pageTurned ? context.pageStartAnchor : context.currentAnchor;
  decision.paused = true;
  return decision;
}

}  // namespace rsvp
