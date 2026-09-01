#include "ArduinoEpubContentProvider.h"

#include <Epub.h>
#include <Print.h>

namespace rsvp {

namespace {

class ContentSinkPrint final : public Print {
 public:
  explicit ContentSinkPrint(ContentByteSink& sink) : sink(sink) {}

  size_t write(const uint8_t byte) override { return sink.write(&byte, 1); }
  size_t write(const uint8_t* bytes, const size_t length) override { return sink.write(bytes, length); }

 private:
  ContentByteSink& sink;
};

}  // namespace

ArduinoEpubContentProvider::ArduinoEpubContentProvider(Epub& epub) : epub(epub) {}

int ArduinoEpubContentProvider::spineCount() const { return epub.getSpineItemsCount(); }

bool ArduinoEpubContentProvider::streamSpine(const uint16_t spineIndex, ContentByteSink& sink,
                                             const size_t chunkSize) {
  if (spineIndex >= static_cast<uint16_t>(epub.getSpineItemsCount())) return false;
  const auto href = epub.getSpineItem(spineIndex).href;
  if (href.empty()) return false;

  ContentSinkPrint output(sink);
  return epub.readItemContentsToStream(href, output, chunkSize, true);
}

}  // namespace rsvp
