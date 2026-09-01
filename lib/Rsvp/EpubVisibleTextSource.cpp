#include "EpubVisibleTextSource.h"

#include <Memory.h>

#include "VisibleContentStream.h"

namespace rsvp {

namespace {

class SelectingSink final : public EventSink {
 public:
  void reset(const uint32_t newMinimumVisibleOffset, const uint32_t newTargetOrdinal) {
    minimumVisibleOffset = newMinimumVisibleOffset;
    targetOrdinal = newTargetOrdinal;
    eligibleOrdinal = 0;
    event = {};
    found = false;
  }

  bool onEvent(const DocumentEvent& candidate) override {
    if (candidate.anchor.visibleTextOffset < minimumVisibleOffset) return true;
    if (eligibleOrdinal++ != targetOrdinal) return true;
    event = candidate;
    found = true;
    return false;
  }

  DocumentEvent event;
  bool found = false;

 private:
  uint32_t minimumVisibleOffset;
  uint32_t targetOrdinal;
  uint32_t eligibleOrdinal = 0;
};

class ParserSink final : public ContentByteSink {
 public:
  explicit ParserSink(VisibleContentStream& parser) : parser(parser) {}

  size_t write(const uint8_t* buffer, const size_t size) override { return parser.write(buffer, size); }

 private:
  VisibleContentStream& parser;
};

}  // namespace

class EpubVisibleTextSource::ScanWorkspace final {
 public:
  ScanWorkspace() : parser(0, sink), output(parser) {}

  void reset(const uint16_t spineIndex, const uint32_t minimumVisibleOffset, const uint32_t targetOrdinal) {
    sink.reset(minimumVisibleOffset, targetOrdinal);
    parser.reset(spineIndex, sink);
  }

  SelectingSink sink;
  VisibleContentStream parser;
  ParserSink output;
};

EpubVisibleTextSource::EpubVisibleTextSource(EpubContentProvider& provider)
    : provider(provider), workspace(makeUniqueNoThrow<ScanWorkspace>()) {}

EpubVisibleTextSource::~EpubVisibleTextSource() = default;

bool EpubVisibleTextSource::open(const ResumeAnchor* anchor) {
  const int totalSpines = provider.spineCount();
  if (!workspace || totalSpines <= 0) return false;

  int initialSpine = 0;
  minimumVisibleOffset = 0;
  if (anchor && anchor->valid && anchor->spineIndex < static_cast<uint16_t>(totalSpines)) {
    initialSpine = anchor->spineIndex;
    minimumVisibleOffset = anchor->visibleTextOffset;
  }
  if (initialSpine < 0 || initialSpine >= totalSpines) return false;

  spineIndex = static_cast<uint16_t>(initialSpine);
  eventOrdinal = 0;
  opened = true;
  return true;
}

bool EpubVisibleTextSource::next(DocumentEvent& out) {
  if (!opened) return false;

  while (spineIndex < static_cast<uint16_t>(provider.spineCount())) {
    bool found = false;
    if (!scanCurrentSpine(out, found)) {
      out = {};
      out.kind = EventKind::Error;
      return true;
    }
    if (found) {
      eventOrdinal++;
      return true;
    }

    spineIndex++;
    minimumVisibleOffset = 0;
    eventOrdinal = 0;
    if (spineIndex < static_cast<uint16_t>(provider.spineCount())) {
      out = {};
      out.kind = EventKind::ChapterBoundary;
      out.anchor = {.spineIndex = spineIndex, .visibleTextOffset = 0, .valid = true};
      return true;
    }
  }

  out = {};
  out.kind = EventKind::EndOfBook;
  out.anchor = {.spineIndex = spineIndex, .visibleTextOffset = 0, .valid = true};
  return true;
}

bool EpubVisibleTextSource::scanCurrentSpine(DocumentEvent& out, bool& found) {
  workspace->reset(spineIndex, minimumVisibleOffset, eventOrdinal);
  if (!provider.streamSpine(spineIndex, workspace->output, 1024)) return false;

  if (!workspace->sink.found) workspace->parser.finish();
  found = workspace->sink.found;
  if (found) out = workspace->sink.event;
  return true;
}

}  // namespace rsvp
