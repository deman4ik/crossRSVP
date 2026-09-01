#pragma once

#include <ArduinoEpubContentProvider.h>
#include <Epub.h>
#include <EpubVisibleTextSource.h>
#include <RsvpRefreshStats.h>
#include <RsvpSession.h>
#include <RsvpWordLayout.h>

#include <atomic>
#include <memory>

#include "ReaderActivity.h"

class RsvpReaderActivity final : public ReaderActivity {
 public:
  explicit RsvpReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                              bool allowFastInitialRefresh)
      : ReaderActivity("RsvpReader", renderer, mappedInput, std::move(bookPath), allowFastInitialRefresh) {}

  void loop() override;

 private:
  bool loadBook() override;
  std::string getBookTitle() const override { return epub ? epub->getTitle() : ""; }
  std::string getBookAuthor() const override { return epub ? epub->getAuthor() : ""; }
  std::string getBookThumbBmpPath() const override { return epub ? epub->getThumbBmpPath() : ""; }
  bool pageTurn(bool) override { return false; }
  bool isAtEndOfBook() const override { return false; }
  void renderBook() override;
  void applyDecision(const rsvp::Decision& decision);
  bool drawPreparedWord(const rsvp::PreparedWord& word);
  void drawStatus() const;
  const char* pauseMessage() const;

  std::unique_ptr<Epub> epub;
  std::unique_ptr<rsvp::ArduinoEpubContentProvider> contentProvider;
  std::unique_ptr<rsvp::EpubVisibleTextSource> source;
  std::unique_ptr<rsvp::RsvpSession> session;
  rsvp::Decision currentDecision;
  bool switchToPagedPending = false;
  std::atomic<bool> wordDoesNotFitPending{false};
  char prefixBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
  char pivotBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
  char suffixBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
};
