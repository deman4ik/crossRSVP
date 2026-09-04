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

static constexpr size_t MAX_CONTEXT_TOKENS_PER_SIDE = 3;

struct ContextToken {
  int advance = 0;
  bool punctuation = false;
};

struct ContextLineInput {
  int focusX = 0;
  int leftBound = 0;
  int rightBound = 0;
  int gap = 0;
  int fontSize = 0;
  int prefixAdvance = 0;
  int pivotAdvance = 0;
  int suffixAdvance = 0;
  std::array<ContextToken, MAX_CONTEXT_TOKENS_PER_SIDE> leftNearest{};
  std::array<ContextToken, MAX_CONTEXT_TOKENS_PER_SIDE> rightNearest{};
  uint8_t leftCount = 0;
  uint8_t rightCount = 0;
};

struct ContextLineLayout {
  RsvpWordLayout active{};
  std::array<int, MAX_CONTEXT_TOKENS_PER_SIDE> leftX{};
  std::array<int, MAX_CONTEXT_TOKENS_PER_SIDE> rightX{};
  std::array<bool, MAX_CONTEXT_TOKENS_PER_SIDE> leftVisible{};
  std::array<bool, MAX_CONTEXT_TOKENS_PER_SIDE> rightVisible{};
  int fontSize = 0;
};

bool calculateRsvpWordLayout(int focusX, int leftBound, int rightBound, int prefixAdvance, int pivotAdvance,
                             int suffixAdvance, RsvpWordLayout& out);

bool calculateRsvpContextLineLayout(const ContextLineInput& input, ContextLineLayout& output);

}  // namespace rsvp
