#pragma once

#include <Epub.h>
#include <ArduinoEpubContentProvider.h>
#include <EpubVisibleTextSource.h>
#include <RsvpRefreshStats.h>
#include <RsvpSession.h>

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

  std::unique_ptr<Epub> epub;
  std::unique_ptr<rsvp::ArduinoEpubContentProvider> contentProvider;
  std::unique_ptr<rsvp::EpubVisibleTextSource> source;
  std::unique_ptr<rsvp::RsvpSession> session;
  rsvp::Decision currentDecision;
  bool switchToPagedPending = false;
};
