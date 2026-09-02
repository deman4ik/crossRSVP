#include "RsvpReaderActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RsvpModeSwitch.h>

#include <algorithm>
#if defined(SIMULATOR)
#include <cstdlib>
#endif
#include <cstring>
#include <optional>

#include "CrossPointSettings.h"
#include "EpubReaderUtils.h"
#include "MappedInputManager.h"
#include "ReaderLaunchMode.h"
#include "ReaderUtils.h"
#include "RsvpCheckpointFile.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
rsvp::RsvpRefreshStats refreshStats;

const char* pauseReasonName(const rsvp::PauseReason reason) {
  switch (reason) {
    case rsvp::PauseReason::Chapter:
      return "chapter";
    case rsvp::PauseReason::Image:
      return "image";
    case rsvp::PauseReason::Table:
      return "table";
    case rsvp::PauseReason::HorizontalRule:
      return "horizontal-rule";
    case rsvp::PauseReason::OtherContent:
      return "other-content";
    case rsvp::PauseReason::OversizedWord:
      return "oversized-word";
    case rsvp::PauseReason::Error:
      return "error";
    case rsvp::PauseReason::None:
      return "none";
  }
  return "unknown";
}
}  // namespace

bool RsvpReaderActivity::loadBook() {
#if defined(SIMULATOR)
  // Native qualification needs a deterministic way to exercise the complete
  // error-screen-to-Paged fallback without corrupting a user's EPUB or cache.
  if (std::getenv("CROSSPOINT_SIM_RSVP_FATAL_LOAD") != nullptr) {
    LOG_ERR("RSVP", "Injected simulator RSVP source-open failure");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
#endif
  bool restoredFromCheckpoint = false;
  uint32_t restoredTokenHash32 = 0;
  uint16_t restoredTokenLength = 0;
  auto loadedEpub = makeUniqueNoThrow<Epub>(bookPath, "/.crosspoint");
  if (!loadedEpub) {
    LOG_ERR("RSVP", "Failed to allocate EPUB object");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }

  const bool uncached = !Storage.exists((loadedEpub->getCachePath() + "/book.bin").c_str());
  if (uncached) disableFastInitialRefresh();

  bool loaded;
  {
    std::optional<GfxRenderer::FrameBufferLoan> loan;
    if (uncached) loan.emplace(renderer);
    loaded = loadedEpub->load(true, SETTINGS.embeddedStyle == 0);
  }
  if (!loaded) {
    LOG_ERR("RSVP", "Failed to load EPUB");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
  loadedEpub->setupCacheDir();
  epub = std::move(loadedEpub);

  rsvp::ResumeAnchor initialAnchor = launchContext.anchor;
  if (!rsvp::RsvpCheckpointFile::computeBookRevision(bookPath, bookRevision)) {
    LOG_ERR("RSVP", "Unable to compute Book Revision; returning to native Paged progress");
    switchToPagedPending = true;
    switchToNativeProgress = true;
  } else if (!initialAnchor.valid) {
    rsvp::RsvpCheckpoint checkpoint;
    const auto checkpointStatus = rsvp::RsvpCheckpointFile::load(epub->getCachePath(), bookRevision, checkpoint);
    if (checkpointStatus == rsvp::CheckpointStatus::Ok) {
      initialAnchor = checkpoint.anchor;
      restoredFromCheckpoint = true;
      restoredTokenHash32 = checkpoint.tokenHash32;
      restoredTokenLength = checkpoint.tokenLength;
      restoredActiveRsvpTimeMs = checkpoint.activeRsvpTimeMs;
    } else if (checkpointStatus != rsvp::CheckpointStatus::Missing) {
      LOG_INF("RSVP", "Ignoring checkpoint status=%d; Paged progress remains authoritative",
              static_cast<int>(checkpointStatus));
      switchToPagedPending = true;
      switchToNativeProgress = true;
      invalidateCheckpointOnNativeFallback = checkpointStatus != rsvp::CheckpointStatus::ReadError;
    }
  }
  if (switchToPagedPending) return true;

  contentProvider = makeUniqueNoThrow<rsvp::ArduinoEpubContentProvider>(*epub);
  if (!contentProvider) {
    LOG_ERR("RSVP", "Failed to allocate EPUB content provider");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
  source = makeUniqueNoThrow<rsvp::EpubVisibleTextSource>(*contentProvider);
  if (!source) {
    LOG_ERR("RSVP", "Failed to allocate visible EPUB stream");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
  rsvp::RsvpPacingConfig pacing;
  pacing.paceWpm = SETTINGS.rsvpPaceWpm;
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
  session = makeUniqueNoThrow<rsvp::RsvpSession>(*source, initialAnchor, pacing);
  if (!session) {
    LOG_ERR("RSVP", "Failed to allocate session");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }

  currentDecision = session->step({.nowMs = millis()});
  if (restoredFromCheckpoint && (!currentDecision.render || session->currentTokenHash() != restoredTokenHash32 ||
                                 session->currentTokenLength() != restoredTokenLength)) {
    LOG_INF("RSVP", "Checkpoint token identity no longer resolves; returning to Paged progress");
    currentDecision = {};
    switchToPagedPending = true;
    switchToNativeProgress = true;
    invalidateCheckpointOnNativeFallback = true;
    return true;
  }
  if (!currentDecision.render && currentDecision.pauseReason == rsvp::PauseReason::None &&
      currentDecision.state != rsvp::State::Finished) {
    LOG_ERR("RSVP", "No readable first token (state=%d error=%d)", static_cast<int>(currentDecision.state),
            static_cast<int>(currentDecision.error));
    return enterFatalFallback(currentDecision.error == rsvp::Error::None ? rsvp::Error::InvalidDocument
                                                                         : currentDecision.error);
  }
  return true;
}

void RsvpReaderActivity::loop() {
  if (fatalFallbackReady.exchange(false)) {
    switchToPagedPending = true;
    switchToPaged();
    return;
  }
  if (switchToPagedPending) {
    switchToPaged();
    return;
  }
  if (checkpointRequestedFromRender.exchange(false)) {
    RenderLock lock(*this);
    if (!saveCheckpoint()) LOG_ERR("RSVP", "Failed to save deferred RSVP checkpoint");
  }
  if (!session) {
    if (fatalFallbackPending.load(std::memory_order_acquire)) return;
    finish();
    return;
  }

  rsvp::Action action = rsvp::Action::None;
  if (wordDoesNotFitPending.exchange(false)) {
    action = rsvp::Action::WordDoesNotFit;
  } else if (mappedInput.wasLongPressed(MappedInputManager::Button::Back, ReaderUtils::GO_BACK_OR_HOME_MS)) {
    if (SETTINGS.backShortToFileBrowser)
      onGoHome();
    else
      activityManager.goToFileBrowser(bookPath);
    return;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back))
    action = rsvp::Action::ModeSwitch;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm))
    action = rsvp::Action::TogglePlayback;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Left))
    action = rsvp::Action::PaceDown;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
    action = rsvp::Action::PaceUp;
  else if (mappedInput.wasReleased(MappedInputManager::Button::PageBack))
    action = rsvp::Action::RewindFive;
  else if (mappedInput.wasReleased(MappedInputManager::Button::PageForward))
    action = rsvp::Action::StepForward;

  rsvp::Decision decision;
  {
    RenderLock lock(*this);
    decision = session->step({.nowMs = millis(), .action = action});
    if (decision.checkpointRequested && !saveCheckpoint()) {
      LOG_ERR("RSVP", "Failed to save RSVP checkpoint");
    }
    applyDecision(decision);
  }
  if (decision.state == rsvp::State::Error) {
    fatalFallbackPending.store(true, std::memory_order_release);
    return;
  }
  if (decision.switchToPaged) {
    switchToPagedPending = true;
    switchToPaged();
    return;
  }
}

bool RsvpReaderActivity::enterFatalFallback(const rsvp::Error error) {
  currentDecision = {};
  currentDecision.state = rsvp::State::Error;
  currentDecision.error = error;
  currentDecision.pauseReason = rsvp::PauseReason::Error;
  fatalFallbackPending.store(true, std::memory_order_release);
  return true;
}

bool RsvpReaderActivity::saveCheckpoint() {
  if (checkpointWritesDisabled || !session || !epub || bookRevision == 0) return false;
  const auto anchor = session->currentAnchor();
  if (!anchor.valid) return false;

  const rsvp::RsvpCheckpoint checkpoint{
      .bookRevision = bookRevision,
      .anchor = anchor,
      .tokenHash32 = session->currentTokenHash(),
      .tokenLength = session->currentTokenLength(),
      .activeRsvpTimeMs = restoredActiveRsvpTimeMs + session->activeReadingMs(),
  };
  const bool checkpointSaved = rsvp::RsvpCheckpointFile::save(epub->getCachePath(), checkpoint);
  if (!checkpointSaved) return false;
  if (lastNativeProgressAnchor.valid && lastNativeProgressAnchor.spineIndex == anchor.spineIndex &&
      lastNativeProgressAnchor.visibleTextOffset == anchor.visibleTextOffset) {
    return true;
  }
  // visibleTextOffset is authoritative when Paged rebuilds the chapter, so a
  // page number is not needed here. This keeps native progress aligned with
  // RSVP even when pagination settings change between modes.
  const bool progressSaved = EpubReaderUtils::saveProgress(*epub, anchor.spineIndex, 0, 0, anchor.visibleTextOffset);
  if (progressSaved) lastNativeProgressAnchor = anchor;
  return progressSaved;
}

void RsvpReaderActivity::switchToPaged() {
  if (switchToNativeProgress) {
    checkpointWritesDisabled = true;
    if (epub && invalidateCheckpointOnNativeFallback) {
      rsvp::RsvpCheckpointFile::invalidate(epub->getCachePath());
    }
    activityManager.goToReader(bookPath, false, ReaderLaunchContext{ReaderLaunchMode::Paged});
    return;
  }
  const auto anchor = session ? session->currentAnchor() : rsvp::ResumeAnchor{};
  const auto decision = rsvp::RsvpModeSwitch::fromRsvp(anchor);
  saveCheckpoint();
  activityManager.goToReader(
      bookPath, false,
      ReaderLaunchContext{ReaderLaunchMode::Paged, decision.anchor, decision.temporaryHighlight && anchor.valid,
                          session ? session->currentTokenHash() : 0,
                          session ? session->currentTokenLength() : uint16_t{0}});
}

void RsvpReaderActivity::onExit() {
  if (!checkpointWritesDisabled) saveCheckpoint();
  ReaderActivity::onExit();
}

void RsvpReaderActivity::applyDecision(const rsvp::Decision& decision) {
  const bool visualStateChanged = decision.state != currentDecision.state ||
                                  decision.paceWpm != currentDecision.paceWpm ||
                                  decision.pauseReason != currentDecision.pauseReason;
  if (decision.render || decision.pauseReason != rsvp::PauseReason::None || decision.state == rsvp::State::Finished ||
      decision.state == rsvp::State::Error) {
    currentDecision = decision;
  } else {
    currentDecision.state = decision.state;
    currentDecision.error = decision.error;
    currentDecision.paceWpm = decision.paceWpm;
    currentDecision.nextDeadlineMs = decision.nextDeadlineMs;
    currentDecision.cleanupRefresh = decision.cleanupRefresh;
  }
  if (decision.render || decision.cleanupRefresh || visualStateChanged) requestUpdate();
}

namespace {
bool copyRange(const rsvp::PreparedWord& word, const uint16_t begin, const uint16_t end, char* output) {
  if (begin > end || end > word.textLength) return false;
  const size_t length = end - begin;
  memcpy(output, word.text + begin, length);
  output[length] = '\0';
  return true;
}
}  // namespace

bool RsvpReaderActivity::drawPreparedWord(const rsvp::PreparedWord& word) {
  if (!word.valid || !copyRange(word, 0, word.pivot.begin, prefixBuffer) ||
      !copyRange(word, word.pivot.begin, word.pivot.end, pivotBuffer) ||
      !copyRange(word, word.pivot.end, word.textLength, suffixBuffer)) {
    return false;
  }

  int marginTop = 0;
  int marginRight = 0;
  int marginBottom = 0;
  int marginLeft = 0;
  renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
  const int left = marginLeft + 12;
  const int right = renderer.getScreenWidth() - marginRight - 12;
  const int focusX = left + (right - left) / 2;

  rsvp::RsvpWordLayout layout;
  int fontId = 0;
  for (int pointSize = SETTINGS.rsvpFontSize; pointSize >= CrossPointSettings::RSVP_FONT_SIZE_MIN;
       pointSize -= CrossPointSettings::RSVP_FONT_SIZE_STEP) {
    fontId = SETTINGS.getReaderFontIdAtSize(static_cast<uint8_t>(pointSize));
    renderer.ensureSdCardFontReady(fontId, word.text, 0x03);
    const int prefixAdvance = renderer.getTextAdvanceX(fontId, prefixBuffer, EpdFontFamily::REGULAR);
    const int pivotAdvance = renderer.getTextAdvanceX(fontId, pivotBuffer, EpdFontFamily::BOLD);
    const int suffixAdvance = renderer.getTextAdvanceX(fontId, suffixBuffer, EpdFontFamily::REGULAR);
    if (rsvp::calculateRsvpWordLayout(focusX, left, right, prefixAdvance, pivotAdvance, suffixAdvance, layout)) break;
  }
  if (!layout.fits || fontId == 0) return false;

  const int usableBottom = renderer.getScreenHeight() - marginBottom;
  const int lineHeight = renderer.getLineHeight(fontId);
  const int y = marginTop + (usableBottom - marginTop - lineHeight) / 2;
  renderer.drawText(fontId, layout.prefixX, y, prefixBuffer, true, EpdFontFamily::REGULAR);
  renderer.drawText(fontId, layout.pivotX, y, pivotBuffer, true, EpdFontFamily::BOLD);
  renderer.drawText(fontId, layout.suffixX, y, suffixBuffer, true, EpdFontFamily::REGULAR);

  if (SETTINGS.rsvpGuideStyle != CrossPointSettings::RSVP_GUIDES_OFF) {
    renderer.drawLine(focusX, std::max(marginTop, y - 22), focusX, std::max(marginTop, y - 8), 2, true);
    renderer.drawLine(focusX, std::min(usableBottom - 1, y + lineHeight + 8), focusX,
                      std::min(usableBottom - 1, y + lineHeight + 22), 2, true);
  }
  return true;
}

const char* RsvpReaderActivity::pauseMessage() const {
  switch (currentDecision.pauseReason) {
    case rsvp::PauseReason::Chapter:
      return tr(STR_RSVP_BOUNDARY_CHAPTER);
    case rsvp::PauseReason::Image:
    case rsvp::PauseReason::Table:
    case rsvp::PauseReason::HorizontalRule:
    case rsvp::PauseReason::OtherContent:
      return tr(STR_RSVP_BOUNDARY_NON_TEXT);
    case rsvp::PauseReason::OversizedWord:
      return tr(STR_RSVP_BOUNDARY_LONG_WORD);
    case rsvp::PauseReason::Error:
      return tr(STR_RSVP_ERROR_SOURCE);
    case rsvp::PauseReason::None:
      return currentDecision.state == rsvp::State::Finished ? tr(STR_END_OF_BOOK) : nullptr;
  }
  return nullptr;
}

void RsvpReaderActivity::drawStatus() const {
  char status[48];
  const char* stateText = currentDecision.state == rsvp::State::Playing ? tr(STR_RSVP_PLAYING) : tr(STR_RSVP_PAUSED);
  snprintf(status, sizeof(status), "%s  %u", stateText, static_cast<unsigned>(currentDecision.paceWpm));
  renderer.drawCenteredText(SMALL_FONT_ID, 12, status);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - renderer.getLineHeight(SMALL_FONT_ID) - 12,
                            tr(STR_RSVP_HINT_MODE_SWITCH));
}

void RsvpReaderActivity::renderBook() {
  if (!currentDecision.render && pauseMessage() == nullptr) return;

  renderer.clearScreen();
  const char* message = pauseMessage();
  if (message) {
    LOG_INF("RSVP", "pause=%s", pauseReasonName(currentDecision.pauseReason));
    int marginTop = 0;
    int marginRight = 0;
    int marginBottom = 0;
    int marginLeft = 0;
    renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
    const int statusClearance = renderer.getLineHeight(SMALL_FONT_ID) + 36;
    const Rect messageBounds(marginLeft + 16, marginTop + statusClearance,
                             renderer.getScreenWidth() - marginLeft - marginRight - 32,
                             renderer.getScreenHeight() - marginTop - marginBottom - statusClearance * 2);
    // Boundary/error screens are infrequent; the wrapped helper may allocate
    // line strings only when a translation exceeds the oriented safe width.
    UITheme::drawCenteredWrappedText(renderer, messageBounds, UI_12_FONT_ID, message, 3, true, EpdFontFamily::BOLD);
  } else if (!currentDecision.frame.preparedWord || !drawPreparedWord(*currentDecision.frame.preparedWord)) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_RSVP_BOUNDARY_LONG_WORD), true,
                              EpdFontFamily::BOLD);
    wordDoesNotFitPending.store(true);
  }
  drawStatus();

  const bool cleanup = forcedRefreshPending || currentDecision.cleanupRefresh;
  forcedRefreshPending = false;
  currentDecision.cleanupRefresh = false;
  const auto kind = cleanup ? rsvp::RefreshKind::Cleanup : rsvp::RefreshKind::Fast;
  const auto mode = cleanup ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH;
  const uint32_t startedAt = millis();
  renderer.displayBuffer(mode);
  const uint32_t duration = millis() - startedAt;
  refreshStats.record(kind, duration);

  const auto& distribution = refreshStats.distribution(kind);
  LOG_INF("RSVP", "refresh=%s duration=%lums n=%lu min=%lu avg=%lu max=%lu buckets=%lu,%lu,%lu,%lu,%lu,%lu heap=%u",
          cleanup ? "cleanup" : "fast", static_cast<unsigned long>(duration),
          static_cast<unsigned long>(distribution.count), static_cast<unsigned long>(distribution.minimumMs),
          static_cast<unsigned long>(distribution.averageMs()), static_cast<unsigned long>(distribution.maximumMs),
          static_cast<unsigned long>(distribution.buckets[0]), static_cast<unsigned long>(distribution.buckets[1]),
          static_cast<unsigned long>(distribution.buckets[2]), static_cast<unsigned long>(distribution.buckets[3]),
          static_cast<unsigned long>(distribution.buckets[4]), static_cast<unsigned long>(distribution.buckets[5]),
          ESP.getFreeHeap());

  if (message == nullptr && !wordDoesNotFitPending.load()) {
    const auto acknowledgement = session->step({.nowMs = millis(),
                                                .action = rsvp::Action::FramePresented,
                                                .presentedFrameId = currentDecision.frame.id,
                                                .refreshDurationMs = duration});
    if (!acknowledgement.presentationAccepted) {
      LOG_ERR("RSVP", "Ignored presentation acknowledgement for frame %lu",
              static_cast<unsigned long>(currentDecision.frame.id));
    }
    if (acknowledgement.checkpointRequested) checkpointRequestedFromRender.store(true);
  }
  if (fatalFallbackPending.load(std::memory_order_acquire)) fatalFallbackReady.store(true);
}
