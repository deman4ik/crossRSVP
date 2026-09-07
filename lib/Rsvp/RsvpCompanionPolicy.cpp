#include "RsvpCompanionPolicy.h"

namespace rsvp {
namespace {

constexpr bool isTrimPunctuation(const uint32_t codepoint) {
  return isRsvpGroupingBoundary(codepoint) || codepoint == '(' || codepoint == ')' || codepoint == '[' ||
         codepoint == ']' || codepoint == '{' || codepoint == '}' || codepoint == '"' || codepoint == '\'' ||
         codepoint == 0x00AB || codepoint == 0x00BB || codepoint == 0x2018 || codepoint == 0x2019 ||
         codepoint == 0x201C || codepoint == 0x201D || codepoint == 0x2039 || codepoint == 0x203A;
}

bool decode(const char* text, const size_t length, size_t& offset, uint32_t& codepoint) {
  if (offset >= length) return false;
  const auto lead = static_cast<unsigned char>(text[offset]);
  size_t width = 1;
  if (lead < 0x80)
    codepoint = lead;
  else if ((lead & 0xE0) == 0xC0)
    width = 2;
  else if ((lead & 0xF0) == 0xE0)
    width = 3;
  else if ((lead & 0xF8) == 0xF0)
    width = 4;
  else
    return false;
  if (offset + width > length) return false;
  if (width > 1) {
    codepoint = lead & ((1u << (8 - width)) - 1u);
    for (size_t index = 1; index < width; ++index) {
      const auto continuation = static_cast<unsigned char>(text[offset + index]);
      if ((continuation & 0xC0) != 0x80) return false;
      codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
  }
  offset += width;
  return true;
}

bool sameLexeme(const char* input, const size_t length, const char* candidate) {
  size_t begin = 0;
  size_t end = length;
  while (begin < end) {
    size_t next = begin;
    uint32_t cp = 0;
    if (!decode(input, end, next, cp) || !isTrimPunctuation(cp)) break;
    begin = next;
  }
  while (end > begin) {
    size_t cursor = begin;
    size_t last = begin;
    uint32_t cp = 0;
    while (cursor < end) {
      last = cursor;
      if (!decode(input, end, cursor, cp)) return false;
    }
    cursor = last;
    if (!decode(input, end, cursor, cp) || !isTrimPunctuation(cp)) break;
    end = last;
  }
  size_t inputAt = begin;
  size_t candidateAt = 0;
  const size_t candidateLength = __builtin_strlen(candidate);
  while (inputAt < end && candidate[candidateAt] != '\0') {
    uint32_t left = 0;
    if (!decode(input, end, inputAt, left)) return false;
    size_t candidateNext = candidateAt;
    uint32_t right = 0;
    if (!decode(candidate, candidateLength, candidateNext, right)) return false;
    if (foldRussianCodepoint(left) != foldRussianCodepoint(right)) return false;
    candidateAt = candidateNext;
  }
  return inputAt == end && candidate[candidateAt] == '\0';
}

}  // namespace

extern constexpr std::array<RsvpCompanionLexeme, RSVP_COMPANION_CORE_COUNT> RSVP_COMPANION_CORE = {{
    {"в", CompanionRole::Forward},        {"во", CompanionRole::Forward},      {"на", CompanionRole::Forward},
    {"с", CompanionRole::Forward},        {"со", CompanionRole::Forward},      {"к", CompanionRole::Forward},
    {"ко", CompanionRole::Forward},       {"по", CompanionRole::Forward},      {"у", CompanionRole::Forward},
    {"из", CompanionRole::Forward},       {"изо", CompanionRole::Forward},     {"от", CompanionRole::Forward},
    {"ото", CompanionRole::Forward},      {"до", CompanionRole::Forward},      {"за", CompanionRole::Forward},
    {"о", CompanionRole::Forward},        {"об", CompanionRole::Forward},      {"обо", CompanionRole::Forward},
    {"для", CompanionRole::Forward},      {"при", CompanionRole::Forward},     {"без", CompanionRole::Forward},
    {"под", CompanionRole::Forward},      {"подо", CompanionRole::Forward},    {"над", CompanionRole::Forward},
    {"про", CompanionRole::Forward},      {"через", CompanionRole::Forward},   {"перед", CompanionRole::Forward},
    {"передо", CompanionRole::Forward},   {"из-за", CompanionRole::Forward},   {"из-под", CompanionRole::Forward},
    {"между", CompanionRole::Forward},    {"после", CompanionRole::Forward},   {"около", CompanionRole::Forward},
    {"среди", CompanionRole::Forward},    {"кроме", CompanionRole::Forward},   {"против", CompanionRole::Forward},
    {"вместо", CompanionRole::Forward},   {"и", CompanionRole::Forward},       {"а", CompanionRole::Forward},
    {"но", CompanionRole::Forward},       {"или", CompanionRole::Forward},     {"либо", CompanionRole::Forward},
    {"не", CompanionRole::Forward},       {"ни", CompanionRole::Forward},      {"же", CompanionRole::Backward},
    {"ж", CompanionRole::Backward},       {"ли", CompanionRole::Backward},     {"ль", CompanionRole::Backward},
    {"бы", CompanionRole::Bidirectional}, {"б", CompanionRole::Bidirectional},
}};

extern constexpr std::array<RsvpCompanionLexeme, RSVP_COMPANION_EXPERIMENTAL_COUNT> RSVP_COMPANION_EXPERIMENTAL = {{
    {"я", CompanionRole::Forward},
    {"ты", CompanionRole::Forward},
    {"он", CompanionRole::Forward},
    {"она", CompanionRole::Forward},
    {"оно", CompanionRole::Forward},
    {"мы", CompanionRole::Forward},
    {"вы", CompanionRole::Forward},
    {"они", CompanionRole::Forward},
}};

CompanionRole classifyRsvpCompanion(const char* text, const size_t length) {
  if (!text || length == 0) return CompanionRole::None;
  for (const auto& entry : RSVP_COMPANION_CORE) {
    if (sameLexeme(text, length, entry.text)) return entry.role;
  }
  for (const auto& entry : RSVP_COMPANION_EXPERIMENTAL) {
    if (sameLexeme(text, length, entry.text)) return entry.role;
  }
  return CompanionRole::None;
}

CompanionRole classifyRsvpCompanion(const std::string_view text) {
  return classifyRsvpCompanion(text.data(), text.size());
}

}  // namespace rsvp
