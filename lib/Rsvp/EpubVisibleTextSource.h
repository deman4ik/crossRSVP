#pragma once

#include <memory>

#include "EpubContentProvider.h"
#include "RsvpTypes.h"

namespace rsvp {

class EpubVisibleTextSource final : public RsvpSource {
 public:
  explicit EpubVisibleTextSource(EpubContentProvider& provider);
  ~EpubVisibleTextSource() override;

  bool open(const ResumeAnchor* anchor = nullptr) override;
  bool next(DocumentEvent& out) override;

 private:
  class ScanWorkspace;

  bool scanCurrentSpine(DocumentEvent& out, bool& found);

  EpubContentProvider& provider;
  std::unique_ptr<ScanWorkspace> workspace;
  uint16_t spineIndex = 0;
  uint32_t minimumVisibleOffset = 0;
  uint32_t eventOrdinal = 0;
  bool opened = false;
};

}  // namespace rsvp
