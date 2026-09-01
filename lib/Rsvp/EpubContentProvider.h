#pragma once

#include <cstddef>
#include <cstdint>

namespace rsvp {

class ContentByteSink {
 public:
  virtual ~ContentByteSink() = default;
  virtual size_t write(const uint8_t* bytes, size_t length) = 0;
};

class EpubContentProvider {
 public:
  virtual ~EpubContentProvider() = default;
  virtual int spineCount() const = 0;
  virtual bool streamSpine(uint16_t spineIndex, ContentByteSink& sink, size_t chunkSize) = 0;
};

}  // namespace rsvp
