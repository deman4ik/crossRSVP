#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rsvp {

enum class CompanionRole : uint8_t { None, Forward, Backward, Bidirectional };

enum class GroupingLanguage : uint8_t { None, Russian, English };
enum class GroupingLanguageChoice : uint8_t { Auto, Russian, English };

struct RsvpCompanionLexeme {
  const char* text;
  CompanionRole role;
};

inline constexpr size_t RSVP_COMPANION_CORE_COUNT = 50;
inline constexpr size_t RSVP_COMPANION_EXPERIMENTAL_COUNT = 8;
inline constexpr size_t RSVP_ENGLISH_COMPANION_CORE_COUNT = 14;
inline constexpr size_t RSVP_ENGLISH_COMPANION_EXPERIMENTAL_COUNT = 34;

// These remain separate so the device trial can narrow the pronoun layer.
extern const std::array<RsvpCompanionLexeme, RSVP_COMPANION_CORE_COUNT> RSVP_COMPANION_CORE;
extern const std::array<RsvpCompanionLexeme, RSVP_COMPANION_EXPERIMENTAL_COUNT> RSVP_COMPANION_EXPERIMENTAL;
extern const std::array<RsvpCompanionLexeme, RSVP_ENGLISH_COMPANION_CORE_COUNT> RSVP_ENGLISH_COMPANION_CORE;
extern const std::array<RsvpCompanionLexeme, RSVP_ENGLISH_COMPANION_EXPERIMENTAL_COUNT>
    RSVP_ENGLISH_COMPANION_EXPERIMENTAL;

// Classifies one exact form without allocating. Leading/trailing punctuation is ignored.
CompanionRole classifyRsvpCompanion(const char* text, size_t length);
CompanionRole classifyRsvpCompanion(std::string_view text);
CompanionRole classifyRsvpCompanion(const char* text, size_t length, GroupingLanguage language);
CompanionRole classifyRsvpCompanion(std::string_view text, GroupingLanguage language);

// Counts Unicode letters in a token. Combining marks and punctuation do not count.
uint8_t rsvpCompanionLetterCount(std::string_view text);
bool isRsvpCompanionEligible(std::string_view text, GroupingLanguage language, uint8_t maximumLetters);
GroupingLanguage resolveGroupingLanguage(GroupingLanguageChoice choice, std::string_view metadata);

// True for punctuation that terminates a grouping segment. It is intentionally
// independent from the lexeme matcher so the resolver can stop before consuming it.
constexpr bool isRsvpGroupingBoundary(uint32_t codepoint) {
  return codepoint == ',' || codepoint == ';' || codepoint == ':' || codepoint == '-' || codepoint == 0x2013 ||
         codepoint == 0x2014 || codepoint == '.' || codepoint == '!' || codepoint == '?' || codepoint == 0x2026 ||
         codepoint == 0x2047 || codepoint == 0x2048 || codepoint == 0x2049 || codepoint == '\n' || codepoint == '\r';
}

constexpr uint32_t foldRussianCodepoint(uint32_t codepoint) {
  if (codepoint >= 'A' && codepoint <= 'Z') return codepoint + ('a' - 'A');
  if (codepoint >= 0x410 && codepoint <= 0x42F) return codepoint + 0x20;
  if (codepoint == 0x401) return 0x451;
  return codepoint;
}

}  // namespace rsvp
