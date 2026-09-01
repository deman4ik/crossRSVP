#pragma once

#include <cstddef>
#include <cstdint>

#include "RsvpTypes.h"

namespace rsvp {

class EventSink {
 public:
  virtual ~EventSink() = default;
  virtual bool onEvent(const DocumentEvent& event) = 0;
};

class VisibleContentStream final {
 public:
  explicit VisibleContentStream(uint16_t spineIndex, EventSink& sink);

  void reset(uint16_t spineIndex, EventSink& sink);
  size_t write(const uint8_t* bytes, size_t length);
  bool finish();
  uint32_t visibleTextOffset() const;

 private:
  static constexpr size_t MAX_TAG_NAME_BYTES = 16;
  static constexpr size_t MAX_ENTITY_BYTES = 16;

  bool processByte(uint8_t byte);
  bool processTextByte(uint8_t byte);
  bool processEntityByte(uint8_t byte);
  bool processTagByte(uint8_t byte);
  bool finishTag();
  bool openTag();
  bool closeTag();
  bool appendVisibleBytes(const char* bytes, size_t length);
  bool appendVisibleCodepoint(const char* bytes, size_t length);
  bool flushWord();
  bool emitBoundary();
  bool emitNonText(NonTextKind kind);
  bool emit(EventKind kind, NonTextKind nonText = NonTextKind::None);

  uint16_t spineIndex;
  EventSink* sink;
  uint32_t offset = 0;
  uint32_t wordOffset = 0;
  uint16_t wordLength = 0;
  char word[MAX_TOKEN_BYTES + 1] = {};
  bool wordOversized = false;
  bool insideTag = false;
  bool tagIsClose = false;
  bool tagNameDone = false;
  bool tagSelfClosing = false;
  bool tagIgnored = false;
  bool tagInQuote = false;
  char tagQuote = 0;
  char tagName[MAX_TAG_NAME_BYTES] = {};
  uint8_t tagNameLength = 0;
  bool insideEntity = false;
  char entity[MAX_ENTITY_BYTES] = {};
  uint8_t entityLength = 0;
  bool insideBody = false;
  uint8_t nonVisibleDepth = 0;
  bool previousVisibleWasCr = false;
  bool emittedVisibleContent = false;
  bool lastEventWasBoundary = true;
  bool stopped = false;
};

}  // namespace rsvp
