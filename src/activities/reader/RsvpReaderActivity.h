#pragma once

#include <ArduinoEpubContentProvider.h>
#include <Epub.h>
#include <EpubVisibleTextSource.h>
#include <RsvpPendingActions.h>
#include <RsvpRefreshStats.h>
#include <RsvpSession.h>
#include <RsvpWordLayout.h>
#include <SdCardFont.h>

#include <atomic>
#include <memory>

#include "ReaderActivity.h"
#include "RsvpControlPanelUi.h"

class RsvpReaderActivity final : public ReaderActivity {
 public:
  explicit RsvpReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                              bool allowFastInitialRefresh, const ReaderLaunchContext& launchContext = {})
      : ReaderActivity("RsvpReader", renderer, mappedInput, std::move(bookPath), allowFastInitialRefresh,
                       launchContext) {}

  void loop() override;
  void onExit() override;
  void onSystemModalOpening() override;

 private:
  bool loadBook() override;
  bool prepareGroupingFonts();
  void applySettings();
  std::string getBookTitle() const override { return epub ? epub->getTitle() : ""; }
  std::string getBookAuthor() const override { return epub ? epub->getAuthor() : ""; }
  std::string getBookThumbBmpPath() const override { return epub ? epub->getThumbBmpPath() : ""; }
  bool pageTurn(bool) override { return false; }
  bool isAtEndOfBook() const override { return false; }
  void renderBook() override;
  void applyDecision(const rsvp::Decision& decision);
  bool measurePresentationGroup(const rsvp::PreparedWord& word, const rsvp::PresentationGroup* group);
  bool drawPreparedWord(const rsvp::PreparedWord& word, const rsvp::PresentationGroup* group);
  static rsvp::GroupRange fitPresentationGroup(void* context, const rsvp::PreparedWord& word,
                                               const rsvp::PresentationGroup& group);
  void drawStatus() const;
  const char* pauseMessage() const;
  bool saveCheckpoint();
  bool saveNativeProgress(const rsvp::ResumeAnchor& anchor);
  bool finalizeCompletedBook();
  void switchToPaged();
  bool enterFatalFallback(rsvp::Error error);

  std::unique_ptr<Epub> epub;
  std::unique_ptr<rsvp::ArduinoEpubContentProvider> contentProvider;
  std::unique_ptr<rsvp::EpubVisibleTextSource> source;
  std::unique_ptr<rsvp::RsvpSession> session;
  rsvp::RsvpPendingActions pendingActions;
  rsvp::Decision currentDecision;
  bool switchToPagedPending = false;
  bool switchToNativeProgress = false;
  bool invalidateCheckpointOnNativeFallback = false;
  bool checkpointWritesDisabled = false;
  bool completionFinalized = false;
  uint64_t bookRevision = 0;
  uint64_t restoredActiveRsvpTimeMs = 0;
  rsvp::ResumeAnchor lastNativeProgressAnchor;
  std::atomic<bool> fatalFallbackPending{false};
  std::atomic<bool> checkpointRequestedFromRender{false};
  std::atomic<bool> fatalFallbackReady{false};
  std::atomic<bool> wordDoesNotFitPending{false};
  std::unique_ptr<SdCardFont::AdvanceBuildScratch> sdFontAdvanceScratch;
  char prefixBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
  char pivotBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
  char suffixBuffer[rsvp::MAX_TOKEN_BYTES + 1] = {};
  rsvp::GroupLayoutInput groupLayoutInput;
  rsvp::GroupLayout groupLayout;
  int activeFontId = 0;
  int companionFontId = 0;
  int smallerSdFontId = 0;
  std::unique_ptr<RsvpControlPanelUi> controlPanel;
  bool panelVisible = false;
  bool swallowTouchRelease = false;
  bool pauseTouchPending = false;
  bool panelDiagnosticsLogged = false;
};
