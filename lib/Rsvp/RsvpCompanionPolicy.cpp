#include "RsvpCompanionPolicy.h"

#include <Utf8.h>

#include <algorithm>
#include <limits>

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

bool isLetter(const uint32_t cp) {
  if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return true;
  if (((cp >= 0x00C0 && cp <= 0x00D6) || (cp >= 0x00D8 && cp <= 0x00F6) || (cp >= 0x00F8 && cp <= 0x00FF)) &&
      cp != 0x00D7 && cp != 0x00F7)
    return true;
  if ((cp >= 0x0100 && cp <= 0x02AF) || (cp >= 0x1E00 && cp <= 0x1EFF)) return true;
  return (cp >= 0x0400 && cp <= 0x0481) || (cp >= 0x048A && cp <= 0x052F);
}

bool isInternalApostropheWord(const char* text, const size_t length) {
  size_t offset = 0;
  bool previousLetter = false;
  bool internal = false;
  while (offset < length) {
    uint32_t cp = 0;
    if (!decode(text, length, offset, cp)) return false;
    if (cp == '\'' || cp == 0x2019) {
      size_t next = offset;
      uint32_t following = 0;
      if (previousLetter && decode(text, length, next, following) && isLetter(following)) internal = true;
      previousLetter = false;
    } else if (!utf8IsCombiningMark(cp)) {
      previousLetter = isLetter(cp);
    }
  }
  return internal;
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

extern constexpr std::array<RsvpCompanionLexeme, RSVP_ENGLISH_COMPANION_CORE_COUNT> RSVP_ENGLISH_COMPANION_CORE = {{
    {"a", CompanionRole::Forward},
    {"an", CompanionRole::Forward},
    {"the", CompanionRole::Forward},
    {"my", CompanionRole::Forward},
    {"your", CompanionRole::Forward},
    {"our", CompanionRole::Forward},
    {"its", CompanionRole::Forward},
    {"their", CompanionRole::Forward},
    {"of", CompanionRole::Forward},
    {"to", CompanionRole::Forward},
    {"from", CompanionRole::Forward},
    {"with", CompanionRole::Forward},
    {"among", CompanionRole::Forward},
    {"during", CompanionRole::Forward},
}};

extern constexpr std::array<RsvpCompanionLexeme, RSVP_ENGLISH_COMPANION_EXPERIMENTAL_COUNT>
    RSVP_ENGLISH_COMPANION_EXPERIMENTAL = {{
        {"at", CompanionRole::Forward},      {"in", CompanionRole::Forward},      {"on", CompanionRole::Forward},
        {"by", CompanionRole::Forward},      {"for", CompanionRole::Forward},     {"into", CompanionRole::Forward},
        {"onto", CompanionRole::Forward},    {"upon", CompanionRole::Forward},    {"under", CompanionRole::Forward},
        {"over", CompanionRole::Forward},    {"about", CompanionRole::Forward},   {"around", CompanionRole::Forward},
        {"behind", CompanionRole::Forward},  {"beside", CompanionRole::Forward},  {"beyond", CompanionRole::Forward},
        {"near", CompanionRole::Forward},    {"toward", CompanionRole::Forward},  {"within", CompanionRole::Forward},
        {"without", CompanionRole::Forward}, {"against", CompanionRole::Forward}, {"between", CompanionRole::Forward},
        {"towards", CompanionRole::Forward}, {"through", CompanionRole::Forward}, {"I", CompanionRole::Forward},
        {"you", CompanionRole::Forward},     {"he", CompanionRole::Forward},      {"she", CompanionRole::Forward},
        {"it", CompanionRole::Forward},      {"we", CompanionRole::Forward},      {"they", CompanionRole::Forward},
        {"and", CompanionRole::Forward},     {"or", CompanionRole::Forward},      {"but", CompanionRole::Forward},
        {"not", CompanionRole::Forward},
    }};

CompanionRole classifyRsvpCompanion(const char* text, const size_t length) {
  if (!text || length == 0) return CompanionRole::None;
  const auto matches = [&](const auto& entry) { return sameLexeme(text, length, entry.text); };
  const auto core = std::find_if(RSVP_COMPANION_CORE.begin(), RSVP_COMPANION_CORE.end(), matches);
  if (core != RSVP_COMPANION_CORE.end()) return core->role;
  const auto extra = std::find_if(RSVP_COMPANION_EXPERIMENTAL.begin(), RSVP_COMPANION_EXPERIMENTAL.end(), matches);
  if (extra != RSVP_COMPANION_EXPERIMENTAL.end()) return extra->role;
  return CompanionRole::None;
}

CompanionRole classifyRsvpCompanion(const std::string_view text) {
  return classifyRsvpCompanion(text.data(), text.size());
}

CompanionRole classifyRsvpCompanion(const char* text, const size_t length, const GroupingLanguage language) {
  if (!text || length == 0) return CompanionRole::None;
  if (language == GroupingLanguage::English) {
    const auto matches = [&](const auto& entry) { return sameLexeme(text, length, entry.text); };
    if (std::any_of(RSVP_ENGLISH_COMPANION_CORE.begin(), RSVP_ENGLISH_COMPANION_CORE.end(), matches) ||
        std::any_of(RSVP_ENGLISH_COMPANION_EXPERIMENTAL.begin(), RSVP_ENGLISH_COMPANION_EXPERIMENTAL.end(), matches))
      return CompanionRole::Forward;
    if (isInternalApostropheWord(text, length)) return CompanionRole::Forward;
    return CompanionRole::None;
  }
  if (language == GroupingLanguage::Russian) return classifyRsvpCompanion(text, length);
  return CompanionRole::None;
}

CompanionRole classifyRsvpCompanion(const std::string_view text, const GroupingLanguage language) {
  return classifyRsvpCompanion(text.data(), text.size(), language);
}

uint8_t rsvpCompanionLetterCount(const std::string_view text) {
  uint8_t count = 0;
  size_t offset = 0;
  while (offset < text.size()) {
    uint32_t cp = 0;
    if (!decode(text.data(), text.size(), offset, cp)) break;
    if (isLetter(cp) && count != std::numeric_limits<uint8_t>::max()) ++count;
  }
  return count;
}

bool isRsvpCompanionEligible(const std::string_view text, const GroupingLanguage language,
                             const uint8_t maximumLetters) {
  return classifyRsvpCompanion(text, language) != CompanionRole::None &&
         rsvpCompanionLetterCount(text) <= maximumLetters;
}

GroupingLanguage resolveGroupingLanguage(const GroupingLanguageChoice choice, std::string_view tag) {
  if (choice == GroupingLanguageChoice::Russian) return GroupingLanguage::Russian;
  if (choice == GroupingLanguageChoice::English) return GroupingLanguage::English;
  const auto whitespace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c; };
  const auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
  const auto digit = [](char c) { return c >= '0' && c <= '9'; };
  while (!tag.empty() && whitespace(tag.front())) tag.remove_prefix(1);
  while (!tag.empty() && whitespace(tag.back())) tag.remove_suffix(1);
  if (tag.size() < 2 || tag.back() == '-') return GroupingLanguage::None;
  const auto language = lower(tag[0]) == 'e' && lower(tag[1]) == 'n'   ? GroupingLanguage::English
                        : lower(tag[0]) == 'r' && lower(tag[1]) == 'u' ? GroupingLanguage::Russian
                                                                       : GroupingLanguage::None;
  if (language == GroupingLanguage::None || tag.size() == 2) return language;
  if (tag[2] != '-') return GroupingLanguage::None;
  tag.remove_prefix(3);

  // Validate RFC 5646 subtag shapes without copying metadata or allocating.
  for (size_t begin = 0; begin < tag.size();) {
    const auto end = tag.find('-', begin);
    const auto part = tag.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (part.empty() || part.size() > 8 ||
        !std::all_of(part.begin(), part.end(), [&](char c) { return alpha(c) || digit(c); }))
      return GroupingLanguage::None;
    begin += part.size() + 1;
  }
  const auto next = [&]() { return tag.substr(0, tag.find('-')); };
  const auto consume = [&]() { tag.remove_prefix(std::min(tag.size(), next().size() + 1)); };
  const auto allAlpha = [&](std::string_view part) { return std::all_of(part.begin(), part.end(), alpha); };
  const auto allDigits = [&](std::string_view part) { return std::all_of(part.begin(), part.end(), digit); };
  // Extended language subtags, optional script, optional region.
  for (unsigned count = 0; count < 3 && next().size() == 3 && allAlpha(next()); ++count) consume();
  if (next().size() == 4 && allAlpha(next())) consume();
  if ((next().size() == 2 && allAlpha(next())) || (next().size() == 3 && allDigits(next()))) consume();
  const auto variants = tag;
  size_t used = 0;
  while (next().size() >= 5 || (next().size() == 4 && digit(next().front()))) {
    const auto part = next();
    for (size_t begin = 0; begin < used;) {
      const auto end = variants.find('-', begin);
      const auto previous = variants.substr(begin, end == std::string_view::npos ? end : end - begin);
      if (previous.size() == part.size() && std::equal(previous.begin(), previous.end(), part.begin(),
                                                       [&](char a, char b) { return lower(a) == lower(b); }))
        return GroupingLanguage::None;
      begin += previous.size() + 1;
    }
    used += part.size() + 1;
    consume();
  }
  uint64_t singletons = 0;
  while (next().size() == 1 && lower(next().front()) != 'x') {
    const char key = lower(next().front());
    const unsigned bit = digit(key) ? static_cast<unsigned>(key - '0') : static_cast<unsigned>(key - 'a' + 10);
    if ((singletons & (uint64_t{1} << bit)) != 0) return GroupingLanguage::None;
    singletons |= uint64_t{1} << bit;
    consume();
    if (next().size() < 2) return GroupingLanguage::None;
    while (next().size() >= 2) consume();
  }
  if (next().size() == 1 && lower(next().front()) == 'x') {
    consume();
    if (tag.empty()) return GroupingLanguage::None;
    return language;  // Private-use subtags have already passed the 1..8 alphanumeric check.
  }
  return tag.empty() ? language : GroupingLanguage::None;
}

}  // namespace rsvp
