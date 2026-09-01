#include "RsvpLexicalCore.h"

#include "Utf8.h"

namespace rsvp {
namespace {

bool decodeAt(const char* text, const size_t length, const size_t offset,
             uint32_t& codepoint, size_t& next) {
  if (offset >= length) return false;
  size_t cursor = offset;
  const unsigned char lead = static_cast<unsigned char>(text[cursor]);
  size_t width = 1;
  if (lead < 0x80) {
    codepoint = lead;
  } else if ((lead >> 5) == 0x6) {
    width = 2;
  } else if ((lead >> 4) == 0xE) {
    width = 3;
  } else if ((lead >> 3) == 0x1E) {
    width = 4;
  } else {
    codepoint = REPLACEMENT_GLYPH;
  }
  if (width != 1) {
    if (cursor + width > length) {
      codepoint = REPLACEMENT_GLYPH;
    } else {
      codepoint = lead & ((1u << (7 - width)) - 1u);
      for (size_t index = 1; index < width; ++index) {
        const unsigned char continuation =
            static_cast<unsigned char>(text[cursor + index]);
        if ((continuation & 0xC0) != 0x80) {
          codepoint = REPLACEMENT_GLYPH;
          width = 1;
          break;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
      }
      const bool overlong = (width == 2 && codepoint < 0x80) ||
                            (width == 3 && codepoint < 0x800) ||
                            (width == 4 && codepoint < 0x10000);
      const bool surrogate = codepoint >= 0xD800 && codepoint <= 0xDFFF;
      if (overlong || surrogate || codepoint > 0x10FFFF) {
        codepoint = REPLACEMENT_GLYPH;
        width = 1;
      }
    }
  }
  next = cursor + width;
  return true;
}

bool isLatinOrCyrillicLetter(const uint32_t codepoint) {
  return (codepoint >= 'A' && codepoint <= 'Z') ||
         (codepoint >= 'a' && codepoint <= 'z') ||
         (codepoint >= 0x00C0 && codepoint <= 0x02AF) ||
         (codepoint >= 0x0400 && codepoint <= 0x052F) ||
         (codepoint >= 0x2DE0 && codepoint <= 0x2DFF) ||
         (codepoint >= 0xA640 && codepoint <= 0xA69F);
}

bool isLexicalBase(const uint32_t codepoint) {
  return isLatinOrCyrillicLetter(codepoint) ||
         (codepoint >= '0' && codepoint <= '9');
}

bool isInternalJoiner(const uint32_t codepoint) {
  return codepoint == '-' || codepoint == 0x2010 || codepoint == 0x2011 ||
         codepoint == '\'' || codepoint == 0x2019 || codepoint == 0x02BC;
}

bool isClosingMark(const uint32_t codepoint) {
  return codepoint == ')' || codepoint == ']' || codepoint == '}' ||
         codepoint == 0x00BB || codepoint == 0x2019 || codepoint == 0x201D ||
         codepoint == 0x203A || codepoint == 0x3009 || codepoint == 0x300B ||
         codepoint == 0x3011 || codepoint == 0x3015 || codepoint == 0x3017 ||
         codepoint == 0x3019 || codepoint == 0x301B;
}

bool previousCodepoint(const char* text, const size_t length, const size_t at,
                       uint32_t& codepoint) {
  size_t cursor = 0;
  size_t previous = 0;
  bool found = false;
  while (cursor < at) {
    previous = cursor;
    size_t next = cursor;
    if (!decodeAt(text, length, cursor, codepoint, next)) return false;
    cursor = next;
    found = previous < at;
  }
  return found && cursor == at;
}

bool nextCodepoint(const char* text, const size_t length, const size_t at,
                   uint32_t& codepoint) {
  size_t next = at;
  return decodeAt(text, length, at, codepoint, next);
}

bool candidateAt(const char* text, const size_t length, const size_t start,
                 const uint32_t codepoint) {
  if (isLexicalBase(codepoint)) return true;
  if (!isInternalJoiner(codepoint)) return false;
  uint32_t previous = 0;
  uint32_t next = 0;
  size_t joinerEnd = start;
  if (!decodeAt(text, length, start, next, joinerEnd)) return false;
  return previousCodepoint(text, length, start, previous) &&
         nextCodepoint(text, length, joinerEnd, next) &&
         isLexicalBase(previous) && isLexicalBase(next);
}

bool lastCodepointBefore(const char* text, const size_t length,
                         const size_t limit, size_t& start,
                         uint32_t& codepoint) {
  size_t cursor = 0;
  bool found = false;
  while (cursor < limit) {
    start = cursor;
    size_t end = cursor;
    if (!decodeAt(text, length, cursor, codepoint, end) || end > limit) {
      return false;
    }
    cursor = end;
    found = true;
  }
  return found;
}

bool isInternalPeriod(const char* text, const size_t length, const size_t start,
                      const size_t end) {
  if (start >= end || text[start] != '.') return false;
  uint32_t previous = 0;
  uint32_t next = 0;
  return previousCodepoint(text, length, start, previous) &&
         nextCodepoint(text, length, end, next) && isLexicalBase(previous) &&
         isLexicalBase(next);
}

bool hasInternalPeriod(const char* text, const size_t length,
                       const size_t before) {
  size_t cursor = 0;
  while (cursor < before) {
    const size_t start = cursor;
    uint32_t codepoint = 0;
    size_t end = cursor;
    if (!decodeAt(text, length, cursor, codepoint, end)) return false;
    if (codepoint == '.' && end <= before &&
        isInternalPeriod(text, length, start, end)) {
      return true;
    }
    cursor = end;
  }
  return false;
}

}  // namespace

bool prepareRsvpWord(const char* source, const size_t sourceLength,
                     PreparedWord& out) {
  out = {};
  if (source == nullptr || sourceLength == 0) return false;

  size_t normalizedLength = 0;
  if (!utf8ComposeNfcToBuffer(source, sourceLength, out.text,
                              sizeof(out.text), normalizedLength, true)) {
    out.overflowed = true;
    return false;
  }
  out.textLength = static_cast<uint16_t>(normalizedLength);

  size_t cursor = 0;
  size_t firstCandidate = normalizedLength;
  size_t lastCandidateEnd = 0;
  size_t candidateCount = 0;
  while (cursor < normalizedLength) {
    const size_t start = cursor;
    uint32_t codepoint = 0;
    size_t end = cursor;
    if (!decodeAt(out.text, normalizedLength, cursor, codepoint, end)) break;
    if (candidateAt(out.text, normalizedLength, start, codepoint)) {
      if (firstCandidate == normalizedLength) firstCandidate = start;
      lastCandidateEnd = end;
      ++candidateCount;
    }
    cursor = end;
  }
  if (candidateCount == 0) return false;

  while (lastCandidateEnd < normalizedLength) {
    uint32_t mark = 0;
    size_t markEnd = lastCandidateEnd;
    if (!decodeAt(out.text, normalizedLength, lastCandidateEnd, mark, markEnd) ||
        !utf8IsCombiningMark(mark)) {
      break;
    }
    lastCandidateEnd = markEnd;
  }

  out.lexicalLength = static_cast<uint16_t>(candidateCount);
  out.prefix = {0, static_cast<uint16_t>(firstCandidate)};
  out.core = {static_cast<uint16_t>(firstCandidate),
              static_cast<uint16_t>(lastCandidateEnd)};
  out.suffix = {static_cast<uint16_t>(lastCandidateEnd), out.textLength};

  // Choose the right member of an even pair so the focus stays on the
  // visual center of the lexical run (the odd case has one middle member).
  const size_t target = candidateCount / 2;
  size_t candidateIndex = 0;
  cursor = firstCandidate;
  while (cursor < normalizedLength) {
    const size_t start = cursor;
    uint32_t codepoint = 0;
    size_t end = cursor;
    if (!decodeAt(out.text, normalizedLength, cursor, codepoint, end)) break;
    if (candidateAt(out.text, normalizedLength, start, codepoint)) {
      if (candidateIndex == target) {
        size_t pivotEnd = end;
        while (pivotEnd < normalizedLength) {
          uint32_t mark = 0;
          size_t markEnd = pivotEnd;
          if (!decodeAt(out.text, normalizedLength, pivotEnd, mark, markEnd) ||
              !utf8IsCombiningMark(mark)) {
            break;
          }
          pivotEnd = markEnd;
        }
        out.pivot = {static_cast<uint16_t>(start),
                     static_cast<uint16_t>(pivotEnd)};
        break;
      }
      ++candidateIndex;
    }
    cursor = end;
  }

  size_t significantEnd = normalizedLength;
  uint32_t significant = 0;
  while (significantEnd > 0) {
    size_t start = 0;
    if (!lastCodepointBefore(out.text, normalizedLength, significantEnd, start,
                             significant)) {
      break;
    }
    if (!isClosingMark(significant)) break;
    significantEnd = start;
  }
  if (significantEnd > 0) {
    size_t start = 0;
    size_t next = 0;
    if (lastCodepointBefore(out.text, normalizedLength, significantEnd, start,
                            significant) &&
        decodeAt(out.text, normalizedLength, start, significant, next)) {
      const size_t finalStart = start;
      if (significant == '!' || significant == '?' || significant == 0x2026 ||
          (significant == '.' &&
           !hasInternalPeriod(out.text, normalizedLength, finalStart))) {
        out.pauseClass = PauseClass::Sentence;
      } else if (significant == ',' || significant == ';' || significant == ':') {
        out.pauseClass = PauseClass::Clause;
      }
    }
  }
  out.valid = true;
  return true;
}

}  // namespace rsvp
