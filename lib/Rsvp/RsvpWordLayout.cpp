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

bool calculateRsvpContextLineLayout(const ContextLineInput& input, ContextLineLayout& output) {
  output = {};
  output.fontSize = input.fontSize;
  if (input.gap < 0 || input.leftCount > MAX_CONTEXT_TOKENS_PER_SIDE ||
      input.rightCount > MAX_CONTEXT_TOKENS_PER_SIDE ||
      !calculateRsvpWordLayout(input.focusX, input.leftBound, input.rightBound, input.prefixAdvance, input.pivotAdvance,
                               input.suffixAdvance, output.active)) {
    return false;
  }

  int cursor = output.active.startX;
  for (uint8_t index = 0; index < input.leftCount; ++index) {
    const int advance = input.leftNearest[index].advance;
    if (advance < 0) return false;
    const int tokenX = cursor - input.gap - advance;
    if (tokenX < input.leftBound) break;
    output.leftX[index] = tokenX;
    output.leftVisible[index] = true;
    cursor = tokenX;
  }

  cursor = output.active.startX + output.active.totalWidth;
  for (uint8_t index = 0; index < input.rightCount; ++index) {
    const int advance = input.rightNearest[index].advance;
    if (advance < 0) return false;
    const int tokenX = cursor + input.gap;
    if (tokenX + advance > input.rightBound) break;
    output.rightX[index] = tokenX;
    output.rightVisible[index] = true;
    cursor = tokenX + advance;
  }
  return true;
}

}  // namespace rsvp
