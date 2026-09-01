#pragma once

namespace rsvp {

struct RsvpWordLayout {
  int startX = 0;
  int prefixX = 0;
  int pivotX = 0;
  int suffixX = 0;
  int prefixAdvance = 0;
  int pivotAdvance = 0;
  int suffixAdvance = 0;
  int totalWidth = 0;
  bool fits = false;
};

bool calculateRsvpWordLayout(int focusX, int leftBound, int rightBound,
                             int prefixAdvance, int pivotAdvance,
                             int suffixAdvance, RsvpWordLayout& out);

}  // namespace rsvp
