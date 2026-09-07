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

bool calculateRsvpGroupLayout(const GroupLayoutInput& input, GroupLayout& output) {
  output = {};
  if (input.gap < 0 || input.count == 0 || input.count > 3 || input.activeIndex >= input.count ||
      !calculateRsvpWordLayout(input.focusX, input.leftBound, input.rightBound, input.prefixAdvance, input.pivotAdvance,
                               input.suffixAdvance, output.active)) {
    return false;
  }

  for (uint8_t index = 0; index < input.count; ++index) {
    if (input.advances[index] < 0) return false;
  }
  output.positions[input.activeIndex] = output.active.startX;
  for (int index = input.activeIndex - 1; index >= 0; --index) {
    output.positions[index] = output.positions[index + 1] - input.gap - input.advances[index];
  }
  int cursor = output.active.startX + output.active.totalWidth;
  for (uint8_t index = input.activeIndex + 1; index < input.count; ++index) {
    output.positions[index] = cursor + input.gap;
    cursor = output.positions[index] + input.advances[index];
  }
  output.begin = 0;
  output.end = input.count;
  while (output.begin < input.activeIndex || output.end > input.activeIndex + 1) {
    const int last = output.end - 1;
    const int right =
        output.positions[last] + (last == input.activeIndex ? output.active.totalWidth : input.advances[last]);
    if (output.positions[output.begin] >= input.leftBound && right <= input.rightBound) break;
    const int leftDistance = input.activeIndex - output.begin;
    const int rightDistance = last - input.activeIndex;
    // Equal-distance ties preserve the postpositive companion.
    if (leftDistance >= rightDistance && leftDistance != 0) {
      ++output.begin;
    } else {
      --output.end;
    }
  }
  return true;
}

}  // namespace rsvp
