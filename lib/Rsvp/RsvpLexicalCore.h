#pragma once

#include <cstddef>
#include <cstdint>

#include "RsvpTypes.h"

namespace rsvp {

struct ByteRange {
  uint16_t begin = 0;
  uint16_t end = 0;

  constexpr uint16_t size() const { return end - begin; }
};

enum class PauseClass : uint8_t { None, Clause, Sentence };
using PunctuationPauseClass = PauseClass;

struct PreparedWord {
  char text[MAX_TOKEN_BYTES + 1] = {};
  uint16_t textLength = 0;
  ByteRange prefix;
  ByteRange core;
  ByteRange pivot;
  ByteRange suffix;
  uint16_t lexicalLength = 0;
  PauseClass pauseClass = PauseClass::None;
  bool valid = false;
  bool overflowed = false;
};

using RsvpPreparedWord = PreparedWord;

bool prepareRsvpWord(const char* source, size_t sourceLength,
                     PreparedWord& out);

}  // namespace rsvp
