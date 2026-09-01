#pragma once

#include "RsvpTypes.h"

namespace rsvp {

class RsvpSession final {
 public:
  explicit RsvpSession(RsvpSource& source, ResumeAnchor initialAnchor = {});

  Decision step(const Input& input);

 private:
  RsvpSource& source;
  ResumeAnchor initialAnchor;
  DocumentEvent currentEvent;
  State state = State::Empty;
  uint32_t frameId = 0;
};

}  // namespace rsvp
