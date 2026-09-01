#include "RsvpWordLayout.h"

namespace rsvp {

bool calculateRsvpWordLayout(const int focusX, const int leftBound, const int rightBound, const int prefixAdvance,
                             const int pivotAdvance, const int suffixAdvance, RsvpWordLayout& out) {
  out.prefixAdvance = prefixAdvance;
  out.pivotAdvance = pivotAdvance;
  out.suffixAdvance = suffixAdvance;
  out.totalWidth = prefixAdvance + pivotAdvance + suffixAdvance;

  // Keep the pivot's integer center anchored at focusX. For odd widths the
  // extra pixel is assigned to the left side, so the rounded center stays on
  // the guide coordinate used by the renderer and tests.
  out.pivotX = focusX - (pivotAdvance + 1) / 2;
  out.startX = out.pivotX - prefixAdvance;
  out.prefixX = out.startX;
  out.suffixX = out.pivotX + pivotAdvance;

  const bool validMetrics = leftBound <= rightBound && prefixAdvance >= 0 && pivotAdvance >= 0 && suffixAdvance >= 0;
  out.fits = validMetrics && out.startX >= leftBound && out.startX + out.totalWidth <= rightBound;
  return out.fits;
}

}  // namespace rsvp
