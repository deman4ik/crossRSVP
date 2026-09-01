#include "VisibleContentStream.h"

#include <cstring>

#include "Epub/VisibleTextUtils.h"
#include "Epub/htmlEntities.h"

namespace rsvp {

namespace {

bool equalsIgnoreCase(const char* lhs, const char* rhs) {
  while (*lhs != '\0' && *rhs != '\0') {
    char left = *lhs++;
    char right = *rhs++;
    if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
    if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
    if (left != right) return false;
  }
  return *lhs == '\0' && *rhs == '\0';
}

bool isWhitespace(const uint8_t byte) {
  return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f';
}

bool isUnicodeWhitespace(const char* bytes, const size_t length) {
  if (length == 2) {
    return static_cast<uint8_t>(bytes[0]) == 0xC2 && static_cast<uint8_t>(bytes[1]) == 0xA0;
  }
  if (length != 3) return false;

  const auto first = static_cast<uint8_t>(bytes[0]);
  const auto second = static_cast<uint8_t>(bytes[1]);
  const auto third = static_cast<uint8_t>(bytes[2]);
  return (first == 0xE2 && second == 0x80 && ((third >= 0x80 && third <= 0x8A) || third == 0xAF)) ||
         (first == 0xE2 && second == 0x81 && third == 0x9F) || (first == 0xE3 && second == 0x80 && third == 0x80);
}

bool isLexicalCodepoint(const uint32_t codepoint) {
  return (codepoint >= '0' && codepoint <= '9') || (codepoint >= 'A' && codepoint <= 'Z') ||
         (codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 0x00C0 && codepoint <= 0x02AF) ||
         (codepoint >= 0x0400 && codepoint <= 0x052F);
}

bool containsLexicalCodepoint(const char* text, const size_t length) {
  for (size_t index = 0; index < length;) {
    const auto first = static_cast<uint8_t>(text[index]);
    uint32_t codepoint = first;
    size_t codepointLength = 1;
    if ((first & 0xE0) == 0xC0 && index + 1 < length) {
      codepoint = static_cast<uint32_t>(first & 0x1F) << 6;
      codepoint |= static_cast<uint8_t>(text[index + 1]) & 0x3F;
      codepointLength = 2;
    } else if ((first & 0xF0) == 0xE0 && index + 2 < length) {
      codepoint = static_cast<uint32_t>(first & 0x0F) << 12;
      codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[index + 1]) & 0x3F) << 6;
      codepoint |= static_cast<uint8_t>(text[index + 2]) & 0x3F;
      codepointLength = 3;
    } else if ((first & 0xF8) == 0xF0 && index + 3 < length) {
      codepoint = static_cast<uint32_t>(first & 0x07) << 18;
      codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[index + 1]) & 0x3F) << 12;
      codepoint |= static_cast<uint32_t>(static_cast<uint8_t>(text[index + 2]) & 0x3F) << 6;
      codepoint |= static_cast<uint8_t>(text[index + 3]) & 0x3F;
      codepointLength = 4;
    }
    if (isLexicalCodepoint(codepoint)) return true;
    index += codepointLength;
  }
  return false;
}

bool isParagraphTag(const char* name) {
  if (equalsIgnoreCase(name, "p") || equalsIgnoreCase(name, "li") || equalsIgnoreCase(name, "div") ||
      equalsIgnoreCase(name, "blockquote")) {
    return true;
  }
  return (name[0] == 'h' || name[0] == 'H') && name[1] >= '1' && name[1] <= '6' && name[2] == '\0';
}

size_t encodeUtf8(uint32_t codepoint, char out[4]) {
  if (codepoint <= 0x7F) {
    out[0] = static_cast<char>(codepoint);
    return 1;
  }
  if (codepoint <= 0x7FF) {
    out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
    out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 2;
  }
  if (codepoint <= 0xFFFF) {
    out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
    out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 3;
  }
  if (codepoint <= 0x10FFFF) {
    out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
    out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 4;
  }
  return 0;
}

bool parseNumericEntity(const char* entity, const size_t length, uint32_t& codepoint) {
  if (length < 4 || entity[0] != '&' || entity[1] != '#' || entity[length - 1] != ';') return false;
  size_t index = 2;
  uint32_t radix = 10;
  if (index < length - 1 && (entity[index] == 'x' || entity[index] == 'X')) {
    radix = 16;
    index++;
  }
  if (index >= length - 1) return false;
  codepoint = 0;
  for (; index < length - 1; index++) {
    const char c = entity[index];
    uint32_t digit;
    if (c >= '0' && c <= '9')
      digit = static_cast<uint32_t>(c - '0');
    else if (radix == 16 && c >= 'a' && c <= 'f')
      digit = static_cast<uint32_t>(c - 'a' + 10);
    else if (radix == 16 && c >= 'A' && c <= 'F')
      digit = static_cast<uint32_t>(c - 'A' + 10);
    else
      return false;
    codepoint = codepoint * radix + digit;
    if (codepoint > 0x10FFFF) return false;
  }
  return codepoint != 0 && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
}

}  // namespace

VisibleContentStream::VisibleContentStream(const uint16_t spineIndex, EventSink& sink)
    : spineIndex(spineIndex), sink(&sink) {}

void VisibleContentStream::reset(const uint16_t newSpineIndex, EventSink& newSink) {
  spineIndex = newSpineIndex;
  sink = &newSink;
  offset = 0;
  wordOffset = 0;
  wordLength = 0;
  word[0] = '\0';
  wordOversized = false;
  joiningDiscretionaryHyphen = false;
  insideTag = false;
  tagIsClose = false;
  tagNameDone = false;
  tagSelfClosing = false;
  tagIgnored = false;
  tagInQuote = false;
  tagQuote = 0;
  tagNameLength = 0;
  tagName[0] = '\0';
  insideEntity = false;
  entityLength = 0;
  entity[0] = '\0';
  insideBody = false;
  nonVisibleDepth = 0;
  previousVisibleWasCr = false;
  emittedVisibleContent = false;
  lastEventWasBoundary = true;
  lastAnchorOffset = 0;
  nextSameOffsetOrdinal = 0;
  anchorOffsetInitialized = false;
  stopped = false;
}

size_t VisibleContentStream::write(const uint8_t* bytes, const size_t length) {
  if (stopped) return 0;
  for (size_t index = 0; index < length; index++) {
    if (!processByte(bytes[index])) {
      stopped = true;
      return index;
    }
  }
  return length;
}

bool VisibleContentStream::finish() {
  if (stopped) return false;
  if (insideEntity) {
    if (!appendVisibleBytes(entity, entityLength)) return false;
    insideEntity = false;
    entityLength = 0;
  }
  if (!flushWord()) return false;
  if (emittedVisibleContent && !lastEventWasBoundary) return emitBoundary();
  return true;
}

uint32_t VisibleContentStream::visibleTextOffset() const { return offset; }

bool VisibleContentStream::processByte(const uint8_t byte) {
  if (insideEntity) return processEntityByte(byte);
  if (insideTag) return processTagByte(byte);
  if (byte == '<') {
    previousVisibleWasCr = false;
    insideTag = true;
    tagIsClose = false;
    tagNameDone = false;
    tagSelfClosing = false;
    tagIgnored = false;
    tagInQuote = false;
    tagQuote = 0;
    tagNameLength = 0;
    tagName[0] = '\0';
    return true;
  }
  if (!insideBody || nonVisibleDepth > 0) {
    previousVisibleWasCr = false;
    return true;
  }
  if (byte == '&') {
    insideEntity = true;
    entity[0] = '&';
    entityLength = 1;
    return true;
  }
  return processTextByte(byte);
}

bool VisibleContentStream::processTextByte(const uint8_t byte) {
  if (VisibleTextUtils::isNormalizedCrLfContinuation(byte, previousVisibleWasCr)) {
    previousVisibleWasCr = false;
    return true;
  }
  previousVisibleWasCr = byte == '\r';
  if (isWhitespace(byte)) {
    if (beginDiscretionaryHyphenJoin(byte) || joiningDiscretionaryHyphen) {
      offset++;
      return true;
    }
    if (!flushWord()) return false;
    offset++;
    return true;
  }
  if (VisibleTextUtils::isUtf8Continuation(byte)) {
    if (!wordOversized && wordLength + 1 <= MAX_TOKEN_BYTES) {
      word[wordLength++] = static_cast<char>(byte);
      word[wordLength] = '\0';
      if (!splitCompletedEmDash()) return false;
    } else {
      wordOversized = true;
    }
    joiningDiscretionaryHyphen = false;
    return true;
  }
  joiningDiscretionaryHyphen = false;
  return appendVisibleCodepoint(reinterpret_cast<const char*>(&byte), 1);
}

bool VisibleContentStream::processEntityByte(const uint8_t byte) {
  if (entityLength + 1 >= MAX_ENTITY_BYTES || byte == '<' || isWhitespace(byte)) {
    if (!appendVisibleBytes(entity, entityLength)) return false;
    insideEntity = false;
    entityLength = 0;
    if (byte == '<') return processByte(byte);
    return processTextByte(byte);
  }

  entity[entityLength++] = static_cast<char>(byte);
  if (byte != ';') return true;

  entity[entityLength] = '\0';
  const char* resolved = lookupHtmlEntity(entity, entityLength);
  bool ok = true;
  if (resolved) {
    ok = appendVisibleBytes(resolved, strlen(resolved));
  } else {
    uint32_t codepoint = 0;
    char encoded[4];
    if (parseNumericEntity(entity, entityLength, codepoint)) {
      ok = appendVisibleCodepoint(encoded, encodeUtf8(codepoint, encoded));
    } else {
      ok = appendVisibleBytes(entity, entityLength);
    }
  }
  insideEntity = false;
  entityLength = 0;
  return ok;
}

bool VisibleContentStream::processTagByte(const uint8_t byte) {
  if (byte == '>' && !tagInQuote) {
    insideTag = false;
    return finishTag();
  }

  if (tagInQuote) {
    if (byte == static_cast<uint8_t>(tagQuote)) {
      tagInQuote = false;
      tagQuote = 0;
    }
    return true;
  }
  if (byte == '"' || byte == '\'') {
    tagInQuote = true;
    tagQuote = static_cast<char>(byte);
    return true;
  }
  if (tagNameLength == 0 && !tagNameDone) {
    if (byte == '/') {
      tagIsClose = true;
      return true;
    }
    if (byte == '!' || byte == '?') {
      tagIgnored = true;
      tagNameDone = true;
      return true;
    }
  }
  if (!tagNameDone) {
    if (isWhitespace(byte) || byte == '/') {
      tagNameDone = true;
      tagName[tagNameLength] = '\0';
      if (byte == '/') tagSelfClosing = true;
    } else if (tagNameLength + 1 < MAX_TAG_NAME_BYTES) {
      tagName[tagNameLength++] = static_cast<char>(byte);
    }
    return true;
  }
  if (byte == '/') tagSelfClosing = true;
  return true;
}

bool VisibleContentStream::finishTag() {
  if (tagIgnored) return true;
  tagName[tagNameLength] = '\0';
  if (tagNameLength == 0) return true;
  if (tagIsClose) return closeTag();
  if (!openTag()) return false;
  if (tagSelfClosing) return closeTag();
  return true;
}

bool VisibleContentStream::openTag() {
  if (equalsIgnoreCase(tagName, "body")) {
    insideBody = true;
    return true;
  }
  if (!insideBody) return true;
  if (nonVisibleDepth > 0 || VisibleTextUtils::isNonVisibleElement(tagName)) {
    nonVisibleDepth++;
    return true;
  }
  if (equalsIgnoreCase(tagName, "img")) return emitNonText(NonTextKind::Image);
  if (equalsIgnoreCase(tagName, "table")) return emitNonText(NonTextKind::Table);
  if (equalsIgnoreCase(tagName, "hr")) return emitNonText(NonTextKind::HorizontalRule);
  if (equalsIgnoreCase(tagName, "svg") || equalsIgnoreCase(tagName, "video") || equalsIgnoreCase(tagName, "audio") ||
      equalsIgnoreCase(tagName, "object") || equalsIgnoreCase(tagName, "canvas") || equalsIgnoreCase(tagName, "math") ||
      equalsIgnoreCase(tagName, "iframe") || equalsIgnoreCase(tagName, "embed")) {
    return emitNonText(NonTextKind::Other);
  }
  if (equalsIgnoreCase(tagName, "br")) return emitBoundary();
  return true;
}

bool VisibleContentStream::closeTag() {
  if (equalsIgnoreCase(tagName, "body")) {
    if (!flushWord()) return false;
    insideBody = false;
    return emittedVisibleContent && !lastEventWasBoundary ? emitBoundary() : true;
  }
  if (!insideBody) return true;
  if (nonVisibleDepth > 0) {
    nonVisibleDepth--;
    return true;
  }
  if (isParagraphTag(tagName)) return emitBoundary();
  return true;
}

bool VisibleContentStream::appendVisibleBytes(const char* bytes, const size_t length) {
  size_t index = 0;
  while (index < length) {
    size_t codepointLength = 1;
    const uint8_t first = static_cast<uint8_t>(bytes[index]);
    if ((first & 0xE0) == 0xC0)
      codepointLength = 2;
    else if ((first & 0xF0) == 0xE0)
      codepointLength = 3;
    else if ((first & 0xF8) == 0xF0)
      codepointLength = 4;
    if (index + codepointLength > length) codepointLength = length - index;
    if (!appendVisibleCodepoint(bytes + index, codepointLength)) return false;
    index += codepointLength;
  }
  return true;
}

bool VisibleContentStream::appendVisibleCodepoint(const char* bytes, const size_t length) {
  if (isUnicodeWhitespace(bytes, length)) {
    if (beginDiscretionaryHyphenJoin('\n') || joiningDiscretionaryHyphen) {
      offset++;
      previousVisibleWasCr = false;
      return true;
    }
    if (!flushWord()) return false;
    offset++;
    previousVisibleWasCr = false;
    return true;
  }
  joiningDiscretionaryHyphen = false;
  if (wordLength == 0 && !wordOversized) wordOffset = offset;
  if (!wordOversized && wordLength + length <= MAX_TOKEN_BYTES) {
    memcpy(word + wordLength, bytes, length);
    wordLength = static_cast<uint16_t>(wordLength + length);
    word[wordLength] = '\0';
  } else {
    wordOversized = true;
  }
  offset++;
  previousVisibleWasCr = false;
  return length == 3 && static_cast<uint8_t>(bytes[0]) == 0xE2 && static_cast<uint8_t>(bytes[1]) == 0x80 &&
                 static_cast<uint8_t>(bytes[2]) == 0x94
             ? splitCompletedEmDash()
             : true;
}

bool VisibleContentStream::splitCompletedEmDash() {
  if (wordOversized || wordLength < 3 || static_cast<uint8_t>(word[wordLength - 3]) != 0xE2 ||
      static_cast<uint8_t>(word[wordLength - 2]) != 0x80 || static_cast<uint8_t>(word[wordLength - 1]) != 0x94) {
    return true;
  }

  wordLength = static_cast<uint16_t>(wordLength - 3);
  word[wordLength] = '\0';
  if (!flushWord()) return false;
  wordOffset = offset - 1;
  word[0] = static_cast<char>(0xE2);
  word[1] = static_cast<char>(0x80);
  word[2] = static_cast<char>(0x94);
  word[3] = '\0';
  wordLength = 3;
  return true;
}

bool VisibleContentStream::beginDiscretionaryHyphenJoin(const uint8_t whitespace) {
  if (wordOversized || wordLength == 0) return false;

  if (wordLength >= 2 && static_cast<uint8_t>(word[wordLength - 2]) == 0xC2 &&
      static_cast<uint8_t>(word[wordLength - 1]) == 0xAD) {
    wordLength = static_cast<uint16_t>(wordLength - 2);
  } else if ((whitespace == '\r' || whitespace == '\n') && word[wordLength - 1] == '-') {
    wordLength--;
  } else {
    return false;
  }
  word[wordLength] = '\0';
  joiningDiscretionaryHyphen = true;
  return true;
}

void VisibleContentStream::assignAnchor(DocumentEvent& event, const uint32_t visibleOffset) {
  if (!anchorOffsetInitialized || lastAnchorOffset != visibleOffset) {
    lastAnchorOffset = visibleOffset;
    nextSameOffsetOrdinal = 0;
    anchorOffsetInitialized = true;
  }
  event.anchor = {.spineIndex = spineIndex,
                  .visibleTextOffset = visibleOffset,
                  .sameOffsetOrdinal = nextSameOffsetOrdinal++,
                  .valid = true};
}

bool VisibleContentStream::flushWord() {
  if (wordLength == 0 && !wordOversized) return true;
  DocumentEvent event;
  event.kind = wordOversized
                   ? EventKind::OversizedWord
                   : (containsLexicalCodepoint(word, wordLength) ? EventKind::Word : EventKind::NonLexicalText);
  assignAnchor(event, wordOffset);
  if (!wordOversized) {
    event.textLength = wordLength;
    memcpy(event.text, word, wordLength + 1);
  }
  wordLength = 0;
  word[0] = '\0';
  wordOversized = false;
  emittedVisibleContent = true;
  lastEventWasBoundary = false;
  return sink->onEvent(event);
}

bool VisibleContentStream::emitBoundary() {
  if (!flushWord()) return false;
  if (!emittedVisibleContent || lastEventWasBoundary) return true;
  return emit(EventKind::ParagraphBoundary);
}

bool VisibleContentStream::emitNonText(const NonTextKind kind) {
  if (!flushWord()) return false;
  return emit(EventKind::NonText, kind);
}

bool VisibleContentStream::emit(const EventKind kind, const NonTextKind nonText) {
  DocumentEvent event;
  event.kind = kind;
  assignAnchor(event, offset);
  event.nonText = nonText;
  emittedVisibleContent = true;
  lastEventWasBoundary = kind == EventKind::ParagraphBoundary;
  return sink->onEvent(event);
}

}  // namespace rsvp
