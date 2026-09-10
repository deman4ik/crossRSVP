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
#include "SdCardFontSystem.h"
#include "activities/ActivityManager.h"
#include "activities/settings/SettingsActivity.h"
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

bool isSkippableNonTextPause(const rsvp::PauseReason reason) {
  return reason == rsvp::PauseReason::Image || reason == rsvp::PauseReason::Table ||
         reason == rsvp::PauseReason::HorizontalRule || reason == rsvp::PauseReason::OtherContent;
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
  bookRevision = launchContext.bookRevision;
  if (bookRevision == 0 && !rsvp::RsvpCheckpointFile::computeBookRevision(bookPath, bookRevision)) {
    LOG_ERR("RSVP", "Unable to compute Book Revision; returning to native Paged progress");
    switchToPagedPending = true;
    switchToNativeProgress = true;
  } else if (launchContext.restoreCheckpoint || !initialAnchor.valid) {
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
  pacing.maximumWpm = pacing.safeMaximumWpm = CrossPointSettings::rsvpMaximumPaceWpm();
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
  const bool groupingEnabled = prepareGroupingFonts();
  session = makeUniqueNoThrow<rsvp::RsvpSession>(*source, initialAnchor, pacing, groupingEnabled,
                                                 &RsvpReaderActivity::fitPresentationGroup, this);
  if (!session) {
    LOG_ERR("RSVP", "Failed to allocate session");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
  if (mappedInput.hasTouch()) {
    controlPanel = makeUniqueNoThrow<RsvpControlPanelUi>(renderer);
    if (!controlPanel) {
      LOG_ERR("RSVP", "OOM: touch control panel");
      return enterFatalFallback(rsvp::Error::SourceOpen);
    } else {
      controlPanel->begin();
      panelVisible = true;
    }
  }
  LOG_INF("RSVP", "short-word-grouping=%s", groupingEnabled ? "on" : "off");

  if (restoredFromCheckpoint) session->restoreAfterCheckpoint(restoredTokenHash32, restoredTokenLength);
  currentDecision = session->step({.nowMs = millis()});
  if (restoredFromCheckpoint && !session->checkpointIdentityValidated()) {
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

bool RsvpReaderActivity::prepareGroupingFonts() {
  bool groupingEnabled = SETTINGS.rsvpShortWordGroupingEnabled != 0;
  bool contextUsesSdFont = false;
  if (groupingEnabled) {
    for (int pointSize = SETTINGS.rsvpFontSize; pointSize >= CrossPointSettings::RSVP_FONT_SIZE_MIN;
         pointSize -= CrossPointSettings::RSVP_FONT_SIZE_STEP) {
      if (renderer.isSdCardFont(SETTINGS.getReaderFontIdAtSize(static_cast<uint8_t>(pointSize)))) {
        contextUsesSdFont = true;
        break;
      }
    }
  }
  if (groupingEnabled && contextUsesSdFont) {
    // Keep the SD advance collector and bounded tables alive for the activity
    // so group measurements do not allocate advance buffers.
    if (!sdFontAdvanceScratch) sdFontAdvanceScratch = makeUniqueNoThrow<SdCardFont::AdvanceBuildScratch>();
    if (!sdFontAdvanceScratch) {
      LOG_ERR("RSVP", "OOM: SD font grouping scratch");
      groupingEnabled = false;
    } else {
      for (int pointSize = SETTINGS.rsvpFontSize; pointSize >= CrossPointSettings::RSVP_FONT_SIZE_MIN;
           pointSize -= CrossPointSettings::RSVP_FONT_SIZE_STEP) {
        const int fontId = SETTINGS.getReaderFontIdAtSize(static_cast<uint8_t>(pointSize));
        if (!renderer.reserveSdCardFontAdvanceTable(fontId, 0x03, SdCardFont::ADVANCE_BUILD_SCRATCH_CODEPOINTS)) {
          LOG_ERR("RSVP", "OOM: SD font grouping advance table");
          sdFontAdvanceScratch.reset();
          groupingEnabled = false;
          break;
        }
      }
    }
  }
  if (groupingEnabled && contextUsesSdFont) {
    smallerSdFontId = sdFontSystem.loadSmallerReaderFont(renderer);
    if (smallerSdFontId == 0 ||
        !renderer.reserveSdCardFontAdvanceTable(smallerSdFontId, 0x01, SdCardFont::ADVANCE_BUILD_SCRATCH_CODEPOINTS)) {
      LOG_ERR("RSVP", "Unable to prepare smaller SD font; disabling grouping");
      groupingEnabled = false;
    }
  }
  return groupingEnabled;
}

void RsvpReaderActivity::applySettings() {
  RenderLock lock;
  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();
  rsvp::RsvpPacingConfig pacing;
  pacing.paceWpm = SETTINGS.rsvpPaceWpm;
  pacing.maximumWpm = pacing.safeMaximumWpm = CrossPointSettings::rsvpMaximumPaceWpm();
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
  if (session) applyDecision(session->configureWhilePaused(pacing, prepareGroupingFonts()));
  if (controlPanel) controlPanel->begin();
  currentDecision.render = true;
  requestUpdate();
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
  if (!session) {
    if (fatalFallbackPending.load(std::memory_order_acquire)) return;
    finish();
    return;
  }

#if defined(SIMULATOR)
  int tappedX = 0;
  int tappedY = 0;
  if (mappedInput.wasScreenTapped(tappedX, tappedY)) {
    LOG_INF("RSVP", "touch_input x=%d y=%d busy=%u", tappedX, tappedY, RenderLock::peek() ? 1u : 0u);
  }
#endif
  const bool swallowedTouchRelease = swallowTouchRelease && mappedInput.wasScreenTouchReleased();
  if (swallowedTouchRelease) swallowTouchRelease = false;
  if (controlPanel && panelVisible && !swallowTouchRelease && !swallowedTouchRelease && !pauseTouchPending) {
    const auto panelEvent = controlPanel->route(mappedInput);
    if (panelEvent != RsvpControlPanelUi::Event::None) {
#if defined(SIMULATOR)
      static constexpr const char* EVENT_NAMES[] = {"none",      "play",    "step",  "rewind5",
                                                    "pace_down", "pace_up", "paged", "settings"};
      LOG_INF("RSVP", "touch_panel_action=%s", EVENT_NAMES[static_cast<unsigned>(panelEvent)]);
#endif
      rsvp::Action action = rsvp::Action::None;
      switch (panelEvent) {
        case RsvpControlPanelUi::Event::Play:
          pendingActions.discard(rsvp::Action::TogglePlayback);
          action = rsvp::Action::TogglePlayback;
          break;
        case RsvpControlPanelUi::Event::Step:
          action = rsvp::Action::StepForward;
          break;
        case RsvpControlPanelUi::Event::Rewind:
          action = rsvp::Action::RewindFive;
          break;
        case RsvpControlPanelUi::Event::PaceDown:
          action = rsvp::Action::PaceDown;
          break;
        case RsvpControlPanelUi::Event::PaceUp:
          action = rsvp::Action::PaceUp;
          break;
        case RsvpControlPanelUi::Event::Paged:
          action = rsvp::Action::ModeSwitch;
          break;
        case RsvpControlPanelUi::Event::Settings: {
          SETTINGS.rsvpPaceWpm = currentDecision.paceWpm;
          auto settings = makeUniqueNoThrow<SettingsActivity>(renderer, mappedInput, true);
          if (!settings) {
            LOG_ERR("RSVP", "OOM: settings activity");
            return;
          }
          onSystemModalOpening();
          startActivityForResult(std::move(settings), [this](const ActivityResult&) { applySettings(); });
        }
          return;
        case RsvpControlPanelUi::Event::None:
          break;
      }
      if (action != rsvp::Action::None && !pendingActions.push(action)) LOG_ERR("RSVP", "Pending action buffer full");
      return;
    }
  }
  if (controlPanel && currentDecision.state == rsvp::State::Playing) {
    int touchX = 0;
    int touchY = 0;
    int marginTop = 0;
    int marginRight = 0;
    int marginBottom = 0;
    int marginLeft = 0;
    renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
    const int systemGestureClearance = marginTop + renderer.getLineHeight(SMALL_FONT_ID) + 24;
    const bool tapped = mappedInput.wasScreenTapped(touchX, touchY);
    if (!pauseTouchPending && (tapped || mappedInput.wasScreenTouchDown(touchX, touchY)) &&
        touchY >= systemGestureClearance) {
      swallowTouchRelease = !tapped;
      pauseTouchPending = true;
#if defined(SIMULATOR)
      LOG_INF("RSVP", "touch_panel_visible=1 reason=working_area_pause x=%d y=%d", touchX, touchY);
#endif
      pendingActions.discard(rsvp::Action::TogglePlayback);
    }
  }

  if (wordDoesNotFitPending.exchange(false)) {
    if (!pendingActions.push(rsvp::Action::WordDoesNotFit)) LOG_ERR("RSVP", "Pending action buffer full");
  }
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Back, ReaderUtils::GO_BACK_OR_HOME_MS)) {
    if (!pendingActions.push(rsvp::Action::Exit)) LOG_ERR("RSVP", "Pending action buffer full");
  } else {
    const auto queueInputAction = [this](const bool triggered, const rsvp::Action action) {
      if (triggered && !pendingActions.push(action)) LOG_ERR("RSVP", "Pending action buffer full");
    };
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Back), rsvp::Action::ModeSwitch);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Confirm), rsvp::Action::TogglePlayback);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Left), rsvp::Action::PaceDown);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Right), rsvp::Action::PaceUp);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::PageBack), rsvp::Action::RewindFive);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::PageForward), rsvp::Action::StepForward);
  }

  if (checkpointRequestedFromRender.load(std::memory_order_acquire)) {
    RenderLock lock(false);
    if (!lock.ownsLock()) return;
    if (checkpointRequestedFromRender.exchange(false)) {
      if (!saveCheckpoint()) LOG_ERR("RSVP", "Failed to save deferred RSVP checkpoint");
    }
  }

  rsvp::Decision decision;
  {
    RenderLock lock(false);
    if (!lock.ownsLock()) return;
    const rsvp::Action action = pauseTouchPending && currentDecision.state == rsvp::State::Playing
                                    ? rsvp::Action::TogglePlayback
                                    : pendingActions.pop();
    if (action == rsvp::Action::Exit) {
      lock.unlock();
      if (SETTINGS.backShortToFileBrowser)
        onGoHome();
      else
        activityManager.goToFileBrowser(bookPath);
      return;
    }

    decision = session->step({.nowMs = millis(), .action = action});
#if defined(SIMULATOR)
    if (action != rsvp::Action::None) {
      LOG_INF("RSVP", "control action=%u state=%u", static_cast<unsigned>(action),
              static_cast<unsigned>(decision.state));
    }
#endif
    if (decision.checkpointRequested && !saveCheckpoint()) {
      LOG_ERR("RSVP", "Failed to save RSVP checkpoint");
    }
    applyDecision(decision);
    if (decision.state == rsvp::State::Finished && !finalizeCompletedBook()) {
      LOG_ERR("RSVP", "Failed to finalize completed book progress");
    }
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
  const bool progressSaved = saveNativeProgress(anchor);
  return checkpointSaved && progressSaved;
}

bool RsvpReaderActivity::saveNativeProgress(const rsvp::ResumeAnchor& anchor) {
  if (!epub || !anchor.valid) return false;
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

bool RsvpReaderActivity::finalizeCompletedBook() {
  if (completionFinalized) return true;
  if (!session || !epub) return false;
  const auto anchor = session->currentAnchor();
  if (anchor.valid && !saveNativeProgress(anchor)) return false;
  if (!rsvp::RsvpCheckpointFile::invalidate(epub->getCachePath())) return false;
  checkpointWritesDisabled = true;
  completionFinalized = true;
  return true;
}

void RsvpReaderActivity::switchToPaged() {
  if (switchToNativeProgress) {
    checkpointWritesDisabled = true;
    bool checkpointInvalidationPending = false;
    if (epub && invalidateCheckpointOnNativeFallback) {
      checkpointInvalidationPending = !rsvp::RsvpCheckpointFile::invalidate(epub->getCachePath());
      if (checkpointInvalidationPending) LOG_ERR("RSVP", "Failed to invalidate rejected RSVP checkpoint");
    }
    activityManager.goToReader(
        bookPath, false,
        ReaderLaunchContext{
            ReaderLaunchMode::Paged, {}, false, 0, 0, false, checkpointInvalidationPending, bookRevision});
    return;
  }
  const auto anchor = session ? session->currentAnchor() : rsvp::ResumeAnchor{};
  const auto decision = rsvp::RsvpModeSwitch::fromRsvp(anchor);
  const bool checkpointSaved = saveCheckpoint();
  activityManager.goToReader(
      bookPath, false,
      ReaderLaunchContext{ReaderLaunchMode::Paged, decision.anchor, decision.temporaryHighlight && anchor.valid,
                          session ? session->currentTokenHash() : 0,
                          session ? session->currentTokenLength() : uint16_t{0},
                          checkpointSaved && SETTINGS.rsvpShortWordGroupingEnabled != 0, false, bookRevision});
}

void RsvpReaderActivity::onExit() {
  if (currentDecision.state == rsvp::State::Finished) {
    if (!finalizeCompletedBook()) LOG_ERR("RSVP", "Failed to finalize completed book on exit");
  } else if (!checkpointWritesDisabled) {
    saveCheckpoint();
  }
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
  if (controlPanel) panelVisible = currentDecision.state != rsvp::State::Playing;
  if (currentDecision.state != rsvp::State::Playing) pauseTouchPending = false;
}

void RsvpReaderActivity::onSystemModalOpening() {
  RenderLock lock;
  if (!session) return;
  // The modal is a pause barrier, including input captured during a refresh.
  pendingActions.discard(rsvp::Action::TogglePlayback);
  pauseTouchPending = false;
  if (currentDecision.state == rsvp::State::Playing) {
    applyDecision(session->step({.nowMs = millis(), .action = rsvp::Action::TogglePlayback}));
  }
  currentDecision.render = true;
  if (controlPanel) panelVisible = true;
#if defined(SIMULATOR)
  LOG_INF("RSVP", "system_modal_pause state=%u", static_cast<unsigned>(currentDecision.state));
#endif
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

bool RsvpReaderActivity::measurePresentationGroup(const rsvp::PreparedWord& word,
                                                  const rsvp::PresentationGroup* group) {
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
  groupLayoutInput = {};
  groupLayoutInput.leftBound = marginLeft + 12;
  groupLayoutInput.rightBound = renderer.getScreenWidth() - marginRight - 12;
  groupLayoutInput.focusX = (groupLayoutInput.leftBound + groupLayoutInput.rightBound) / 2;
  const char* frameText[3] = {word.text};
  size_t frameTextCount = 1;
  if (group && group->count != 0 && group->count <= 3 && group->activeIndex < group->count) {
    frameTextCount = group->count;
    for (uint8_t index = 0; index < group->count; ++index) frameText[index] = group->tokens[index].text;
    groupLayoutInput.count = group->count;
    groupLayoutInput.activeIndex = group->activeIndex;
  }

  rsvp::RsvpWordLayout active;
  int resolvedPointSize = 0;
  for (int pointSize = SETTINGS.rsvpFontSize; pointSize >= CrossPointSettings::RSVP_FONT_SIZE_MIN;
       pointSize -= CrossPointSettings::RSVP_FONT_SIZE_STEP) {
    activeFontId = SETTINGS.getReaderFontIdAtSize(static_cast<uint8_t>(pointSize));
    if (sdFontAdvanceScratch) {
      renderer.ensureSdCardFontReady(activeFontId, frameText, frameTextCount, *sdFontAdvanceScratch, 0x03);
    } else {
      renderer.ensureSdCardFontReady(activeFontId, frameText, frameTextCount, 0x03);
    }
    groupLayoutInput.prefixAdvance = renderer.getTextAdvanceX(activeFontId, prefixBuffer, EpdFontFamily::REGULAR);
    groupLayoutInput.pivotAdvance = renderer.getTextAdvanceX(activeFontId, pivotBuffer, EpdFontFamily::BOLD);
    groupLayoutInput.suffixAdvance = renderer.getTextAdvanceX(activeFontId, suffixBuffer, EpdFontFamily::REGULAR);
    if (rsvp::calculateRsvpWordLayout(groupLayoutInput.focusX, groupLayoutInput.leftBound, groupLayoutInput.rightBound,
                                      groupLayoutInput.prefixAdvance, groupLayoutInput.pivotAdvance,
                                      groupLayoutInput.suffixAdvance, active)) {
      resolvedPointSize = pointSize;
      break;
    }
  }
  if (!active.fits || activeFontId == 0) return false;

  companionFontId = activeFontId;
  if (frameTextCount > 1) {
    companionFontId = renderer.isSdCardFont(activeFontId)
                          ? smallerSdFontId
                          : SETTINGS.getReaderFontIdAtSize(static_cast<uint8_t>(
                                std::max<int>(CrossPointSettings::RSVP_FONT_SIZE_MIN,
                                              resolvedPointSize - CrossPointSettings::RSVP_FONT_SIZE_STEP)));
    if (companionFontId == 0) return false;
    if (sdFontAdvanceScratch) {
      renderer.ensureSdCardFontReady(companionFontId, frameText, frameTextCount, *sdFontAdvanceScratch,
                                     companionFontId == activeFontId ? 0x03 : 0x01);
    } else {
      renderer.ensureSdCardFontReady(companionFontId, frameText, frameTextCount, 0x01);
    }
  }
  groupLayoutInput.gap = renderer.getTextAdvanceX(companionFontId, " ", EpdFontFamily::REGULAR);
  if (group) {
    for (uint8_t index = 0; index < groupLayoutInput.count; ++index) {
      if (index != groupLayoutInput.activeIndex) {
        groupLayoutInput.advances[index] =
            renderer.getTextAdvanceX(companionFontId, group->tokens[index].text, EpdFontFamily::REGULAR);
      }
    }
  }
  return rsvp::calculateRsvpGroupLayout(groupLayoutInput, groupLayout);
}

rsvp::GroupRange RsvpReaderActivity::fitPresentationGroup(void* context, const rsvp::PreparedWord& word,
                                                          const rsvp::PresentationGroup& group) {
  auto& activity = *static_cast<RsvpReaderActivity*>(context);
  if (!activity.measurePresentationGroup(word, &group)) {
    return {group.activeIndex, static_cast<uint8_t>(group.activeIndex + 1)};
  }
  return {activity.groupLayout.begin, activity.groupLayout.end};
}

bool RsvpReaderActivity::drawPreparedWord(const rsvp::PreparedWord& word, const rsvp::PresentationGroup* group) {
  if (!measurePresentationGroup(word, group)) return false;
  // Membership was resolved by the session before source consumption.
  if (group && (groupLayout.begin != 0 || groupLayout.end != group->count)) return false;

  int marginTop = 0;
  int marginRight = 0;
  int marginBottom = 0;
  int marginLeft = 0;
  renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
  const int usableBottom = renderer.getScreenHeight() - marginBottom;
  const int lineHeight = renderer.getLineHeight(activeFontId);
  const int reservedBottom = controlPanel ? controlPanel->reservedHeight() : 0;
  const int y = marginTop + (usableBottom - reservedBottom - marginTop - lineHeight) / 2;
  const int companionY = y + (lineHeight - renderer.getLineHeight(companionFontId)) / 2;
  if (group) {
    for (uint8_t index = 0; index < group->count; ++index) {
      if (index == group->activeIndex) continue;
      renderer.drawText(companionFontId, groupLayout.positions[index], companionY, group->tokens[index].text, true,
                        EpdFontFamily::REGULAR);
    }
  }
  renderer.drawText(activeFontId, groupLayout.active.prefixX, y, prefixBuffer, true, EpdFontFamily::REGULAR);
  renderer.drawText(activeFontId, groupLayout.active.pivotX, y, pivotBuffer, true, EpdFontFamily::BOLD);
  renderer.drawText(activeFontId, groupLayout.active.suffixX, y, suffixBuffer, true, EpdFontFamily::REGULAR);

  if (SETTINGS.rsvpGuideStyle != CrossPointSettings::RSVP_GUIDES_OFF) {
    const int focusX = groupLayoutInput.focusX;
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
  const char* hint = isSkippableNonTextPause(currentDecision.pauseReason) ? tr(STR_RSVP_HINT_BOUNDARY_SKIP)
                                                                          : tr(STR_RSVP_HINT_MODE_SWITCH);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - renderer.getLineHeight(SMALL_FONT_ID) - 12,
                            hint);
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
    const int reservedBottom = controlPanel ? controlPanel->reservedHeight() : 0;
    const Rect messageBounds(
        marginLeft + 16, marginTop + statusClearance, renderer.getScreenWidth() - marginLeft - marginRight - 32,
        renderer.getScreenHeight() - marginTop - marginBottom - statusClearance * 2 - reservedBottom);
    // Boundary/error screens are infrequent; the wrapped helper may allocate
    // line strings only when a translation exceeds the oriented safe width.
    UITheme::drawCenteredWrappedText(renderer, messageBounds, UI_12_FONT_ID, message, 3, true, EpdFontFamily::BOLD);
  } else if (!currentDecision.frame.preparedWord ||
             !drawPreparedWord(*currentDecision.frame.preparedWord, currentDecision.frame.presentationGroup)) {
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_RSVP_BOUNDARY_LONG_WORD), true,
                              EpdFontFamily::BOLD);
    wordDoesNotFitPending.store(true);
  }
  drawStatus();
  if (controlPanel && panelVisible) {
    const bool canPlay = currentDecision.state == rsvp::State::Paused;
    const bool canStep = canPlay || (currentDecision.state == rsvp::State::Boundary &&
                                     isSkippableNonTextPause(currentDecision.pauseReason));
    controlPanel->render(canPlay, canStep);
#if defined(SIMULATOR)
    if (!panelDiagnosticsLogged) {
      const int width = renderer.getScreenWidth();
      const int height = renderer.getScreenHeight();
      const int panelTop = height - controlPanel->reservedHeight();
      LOG_INF("RSVP", "touch_panel_geometry x=0 y=%d w=%d h=%d", panelTop, width, controlPanel->reservedHeight());
      panelDiagnosticsLogged = true;
    }
#endif
  } else {
    panelDiagnosticsLogged = false;
  }

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
#ifdef SIMULATOR
    const auto* group = currentDecision.frame.presentationGroup;
    if (group) {
      LOG_INF("RSVP", "group count=%u active=%u words=%s|%s|%s", group->count, group->activeIndex,
              group->tokens[0].text, group->count > 1 ? group->tokens[1].text : "",
              group->count > 2 ? group->tokens[2].text : "");
    }
#endif
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
