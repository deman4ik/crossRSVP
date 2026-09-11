#pragma once

#include <RsvpTypes.h>

#include <cstring>

namespace rsvp {

// Flash-backed deterministic text for the diagnostic. Reopening at anchor zero
// produces the exact same endless sequence without touching a real reading position.
class RsvpWindowFixtureSource final : public RsvpSource {
 public:
  bool open(const ResumeAnchor* anchor = nullptr) override {
    cursor = anchor && anchor->valid ? anchor->visibleTextOffset % WORD_COUNT : 0;
    return true;
  }

  bool next(DocumentEvent& out) override {
    const char* word = WORDS[cursor];
    const size_t length = strlen(word);
    out = {};
    out.kind = EventKind::Word;
    out.anchor = {.spineIndex = 0, .visibleTextOffset = cursor, .sameOffsetOrdinal = 0, .valid = true};
    out.textLength = static_cast<uint16_t>(length);
    memcpy(out.text, word, length + 1);
    cursor = (cursor + 1) % WORD_COUNT;
    return true;
  }

 private:
  static constexpr uint32_t WORD_COUNT = 16;
  static constexpr const char* WORDS[WORD_COUNT] = {
      "ember", "a",      "horizon", "to",    "quartz", "river", "in",       "lantern",
      "stone", "silver", "we",      "field", "bright", "under", "crossing", "home",
  };
  uint32_t cursor = 0;
};

static_assert(sizeof(RsvpWindowFixtureSource) <= 16);

}  // namespace rsvp
