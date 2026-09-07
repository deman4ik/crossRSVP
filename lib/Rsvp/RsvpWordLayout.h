#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

struct GroupLayoutInput {
  int focusX = 0;
  int leftBound = 0;
  int rightBound = 0;
  int gap = 0;
  int prefixAdvance = 0;
  int pivotAdvance = 0;
  int suffixAdvance = 0;
  std::array<int, 3> advances{};
  uint8_t count = 1;
  uint8_t activeIndex = 0;
};

struct GroupLayout {
  RsvpWordLayout active{};
  std::array<int, 3> positions{};
  uint8_t begin = 0;
  uint8_t end = 0;
};

bool calculateRsvpWordLayout(int focusX, int leftBound, int rightBound, int prefixAdvance, int pivotAdvance,
                             int suffixAdvance, RsvpWordLayout& out);

bool calculateRsvpGroupLayout(const GroupLayoutInput& input, GroupLayout& output);

}  // namespace rsvp
