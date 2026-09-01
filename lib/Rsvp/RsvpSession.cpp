#include "RsvpSession.h"

namespace rsvp {

RsvpSession::RsvpSession(RsvpSource& source, const ResumeAnchor initialAnchor)
    : source(source), initialAnchor(initialAnchor) {}

Decision RsvpSession::step(const Input& input) {
  Decision decision;
  decision.state = state;

  if (state == State::Empty) {
    if (!source.open(initialAnchor.valid ? &initialAnchor : nullptr)) {
      state = State::Error;
      decision.state = state;
      decision.error = Error::SourceOpen;
      return decision;
    }

    while (source.next(currentEvent)) {
      if (currentEvent.kind == EventKind::Word) {
        state = State::Paused;
        decision.state = state;
        decision.render = true;
        decision.frame.id = ++frameId;
        decision.frame.requestedAtMs = input.nowMs;
        decision.frame.text = currentEvent.text;
        decision.frame.textLength = currentEvent.textLength;
        decision.frame.anchor = currentEvent.anchor;
        return decision;
      }
      if (currentEvent.kind == EventKind::EndOfBook) {
        state = State::Finished;
        decision.state = state;
        return decision;
      }
      if (currentEvent.kind == EventKind::Error) {
        state = State::Error;
        decision.state = state;
        decision.error = Error::SourceRead;
        return decision;
      }
      if (currentEvent.kind == EventKind::NonText || currentEvent.kind == EventKind::OversizedWord) {
        state = State::Boundary;
        decision.state = state;
        decision.switchToPaged = true;
        return decision;
      }
    }

    state = State::Error;
    decision.state = state;
    decision.error = Error::SourceRead;
    return decision;
  }

  if (input.action == Action::FramePresented && frameId != 0 && input.presentedFrameId == frameId) {
    decision.presentationAccepted = true;
    decision.presentedAtMs = input.nowMs;
    decision.refreshDurationMs = input.refreshDurationMs;
  } else if (input.action == Action::ModeSwitch) {
    state = State::Exited;
    decision.state = state;
    decision.switchToPaged = true;
  }
  return decision;
}

}  // namespace rsvp
