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
  const bool usePageStart = context.explicitNavigation || context.checkpointRestoreSuppressed;
  decision.anchor = usePageStart ? context.pageStartAnchor : context.currentAnchor;
  if (!usePageStart && !context.currentAnchor.valid) {
    decision.anchor = context.pageStartAnchor;
    decision.restoreCheckpoint = true;
  }
  decision.paused = true;
  return decision;
}

}  // namespace rsvp
