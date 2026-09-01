#pragma once

#include "EpubContentProvider.h"

class Epub;

namespace rsvp {

class ArduinoEpubContentProvider final : public EpubContentProvider {
 public:
  explicit ArduinoEpubContentProvider(Epub& epub);

  int spineCount() const override;
  bool streamSpine(uint16_t spineIndex, ContentByteSink& sink, size_t chunkSize) override;

 private:
  Epub& epub;
};

}  // namespace rsvp
