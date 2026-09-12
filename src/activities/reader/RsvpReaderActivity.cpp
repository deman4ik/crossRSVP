#include "RsvpReaderActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RsvpModeSwitch.h>
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
#include <RsvpWindowReport.h>
#endif

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

GfxRenderer::LogicalRegion unionRegions(const GfxRenderer::LogicalRegion& lhs, const GfxRenderer::LogicalRegion& rhs) {
  const int32_t left = std::min(lhs.x, rhs.x);
  const int32_t top = std::min(lhs.y, rhs.y);
  const int32_t right = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  const int32_t bottom = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return {left, top, right - left, bottom - top};
}

#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
constexpr char WINDOW_REPORT_PATH[] = "/.crosspoint/rsvp-window-test.csv";

rsvp::WindowActualRefresh actualRefresh(const HalDisplay::DisplayUpdateKind kind) {
  switch (kind) {
    case HalDisplay::DisplayUpdateKind::Window:
      return rsvp::WindowActualRefresh::Window;
    case HalDisplay::DisplayUpdateKind::Full:
      return rsvp::WindowActualRefresh::Full;
    case HalDisplay::DisplayUpdateKind::Cleanup:
      return rsvp::WindowActualRefresh::Cleanup;
    case HalDisplay::DisplayUpdateKind::None:
      return rsvp::WindowActualRefresh::None;
  }
  return rsvp::WindowActualRefresh::None;
}

rsvp::WindowActualRefresh requestedRefresh(const HalDisplay::DisplayUpdateKind kind) { return actualRefresh(kind); }

rsvp::WindowDiagnosticError diagnosticError(const HalDisplay::DisplayUpdateError error) {
  switch (error) {
    case HalDisplay::DisplayUpdateError::None:
      return rsvp::WindowDiagnosticError::None;
    case HalDisplay::DisplayUpdateError::InvalidRegion:
      return rsvp::WindowDiagnosticError::InvalidRegion;
    case HalDisplay::DisplayUpdateError::BusyNotReady:
      return rsvp::WindowDiagnosticError::BusyNotReady;
    case HalDisplay::DisplayUpdateError::BusyTimeout:
      return rsvp::WindowDiagnosticError::BusyTimeout;
  }
  return rsvp::WindowDiagnosticError::Other;
}

rsvp::WindowDiagnosticTrace diagnosticTrace(const GfxRenderer::DisplayUpdateTrace& source) {
  return {.valid = source.valid,
          .phase = static_cast<uint8_t>(source.phase),
          .stage = static_cast<uint8_t>(source.stage),
          .error = static_cast<uint8_t>(source.error),
          .wait = static_cast<uint8_t>(source.wait),
          .busyBefore = source.busyBefore,
          .busyAfter = source.busyAfter,
          .baselineBefore = static_cast<uint8_t>(source.baselineBefore),
          .baselineAfter = static_cast<uint8_t>(source.baselineAfter),
          .x = source.x,
          .y = source.y,
          .width = source.width,
          .height = source.height,
          .payloadBytes = source.payloadBytes,
          .refreshTriggered = source.refreshTriggered};
}

rsvp::WindowFallback windowFallback(const HalDisplay::DisplayUpdateResult& result) {
  if (result.error == HalDisplay::DisplayUpdateError::BusyNotReady) return rsvp::WindowFallback::BusyNotReady;
  if (result.error == HalDisplay::DisplayUpdateError::BusyTimeout) return rsvp::WindowFallback::BusyTimeout;
  if (result.error == HalDisplay::DisplayUpdateError::InvalidRegion) return rsvp::WindowFallback::InvalidGeometry;
  if (result.error != HalDisplay::DisplayUpdateError::None) return rsvp::WindowFallback::ControllerError;
  switch (result.fallback) {
    case HalDisplay::DisplayUpdateFallback::None:
      return rsvp::WindowFallback::None;
    case HalDisplay::DisplayUpdateFallback::ExperimentalDisabled:
      return rsvp::WindowFallback::CandidateDisabled;
    case HalDisplay::DisplayUpdateFallback::UnsupportedModel:
      return rsvp::WindowFallback::UnsupportedModel;
    case HalDisplay::DisplayUpdateFallback::UnsupportedController:
    case HalDisplay::DisplayUpdateFallback::UnsupportedDriver:
      return rsvp::WindowFallback::UnsupportedController;
    case HalDisplay::DisplayUpdateFallback::InconclusiveController:
      return rsvp::WindowFallback::InconclusiveController;
    case HalDisplay::DisplayUpdateFallback::Inverted:
      return rsvp::WindowFallback::Inverted;
    case HalDisplay::DisplayUpdateFallback::BaselineInvalid:
    case HalDisplay::DisplayUpdateFallback::RefreshPromoted:
      return rsvp::WindowFallback::InvalidBaseline;
  }
  return rsvp::WindowFallback::ControllerError;
}

const char* controllerName(const HalDisplay::Controller controller) {
  switch (controller) {
    case HalDisplay::Controller::UC8253:
      return "uc8253";
    case HalDisplay::Controller::UC8279:
      return "uc8279";
    case HalDisplay::Controller::UC8179:
      return "uc8179";
    default:
      return "other";
  }
}

const char* confidenceName(const HalDisplay::ControllerConfidence confidence) {
  switch (confidence) {
    case HalDisplay::ControllerConfidence::Confirmed:
      return "confirmed";
    case HalDisplay::ControllerConfidence::Assumed:
      return "assumed";
    case HalDisplay::ControllerConfidence::Inconclusive:
      return "inconclusive";
  }
  return "inconclusive";
}

const char* confidenceDisplayName(const HalDisplay::ControllerConfidence confidence) {
  switch (confidence) {
    case HalDisplay::ControllerConfidence::Confirmed:
      return tr(STR_RSVP_WINDOW_CONFIDENCE_CONFIRMED);
    case HalDisplay::ControllerConfidence::Assumed:
      return tr(STR_RSVP_WINDOW_CONFIDENCE_ASSUMED);
    case HalDisplay::ControllerConfidence::Inconclusive:
      return tr(STR_RSVP_WINDOW_CONFIDENCE_INCONCLUSIVE);
  }
  return tr(STR_RSVP_WINDOW_CONFIDENCE_INCONCLUSIVE);
}

const char* fallbackDisplayName(const rsvp::WindowFallback fallback) {
  switch (fallback) {
    case rsvp::WindowFallback::None:
      return tr(STR_RSVP_WINDOW_FALLBACK_NONE);
    case rsvp::WindowFallback::CandidateDisabled:
      return tr(STR_RSVP_WINDOW_FALLBACK_DISABLED);
    case rsvp::WindowFallback::UnsupportedModel:
      return tr(STR_RSVP_WINDOW_FALLBACK_MODEL);
    case rsvp::WindowFallback::UnsupportedController:
      return tr(STR_RSVP_WINDOW_FALLBACK_CONTROLLER);
    case rsvp::WindowFallback::InconclusiveController:
      return tr(STR_RSVP_WINDOW_FALLBACK_INCONCLUSIVE);
    case rsvp::WindowFallback::Inverted:
      return tr(STR_RSVP_WINDOW_FALLBACK_INVERTED);
    case rsvp::WindowFallback::InvalidGeometry:
      return tr(STR_RSVP_WINDOW_FALLBACK_GEOMETRY);
    case rsvp::WindowFallback::InvalidBaseline:
      return tr(STR_RSVP_WINDOW_FALLBACK_BASELINE);
    case rsvp::WindowFallback::BusyNotReady:
      return tr(STR_RSVP_WINDOW_FALLBACK_ERROR);
    case rsvp::WindowFallback::BusyTimeout:
      return tr(STR_RSVP_WINDOW_FALLBACK_TIMEOUT);
    case rsvp::WindowFallback::ControllerError:
      return tr(STR_RSVP_WINDOW_FALLBACK_ERROR);
  }
  return tr(STR_RSVP_WINDOW_FALLBACK_ERROR);
}

const char* orientationName(const GfxRenderer::Orientation orientation) {
  switch (orientation) {
    case GfxRenderer::Orientation::Portrait:
      return "portrait";
    case GfxRenderer::Orientation::LandscapeClockwise:
      return "landscape_cw";
    case GfxRenderer::Orientation::PortraitInverted:
      return "portrait_inverted";
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      return "landscape_ccw";
  }
  return "unknown";
}

uint16_t observedCpuMhz() {
#if defined(SIMULATOR)
  return 0;  // The simulator exposes no CPU-frequency model.
#else
  return static_cast<uint16_t>(getCpuFrequencyMhz());
#endif
}

class HalFileReportSink final : public rsvp::WindowReportSink {
 public:
  explicit HalFileReportSink(HalFile& file) : file(file) {}
  bool write(const char* data, const size_t size) override {
    return file.write(reinterpret_cast<const uint8_t*>(data), size) == size;
  }

 private:
  HalFile& file;
};
#endif
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

#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
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
#endif
  rsvp::RsvpPacingConfig pacing;
  pacing.paceWpm = SETTINGS.rsvpPaceWpm;
  pacing.maximumWpm = pacing.safeMaximumWpm = CrossPointSettings::rsvpMaximumPaceWpm();
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  pacing.paceWpm = rsvp::RsvpSpeedProbe::DEFAULT_WPM;
  pacing.minimumWpm = rsvp::RsvpSpeedProbe::MINIMUM_WPM;
  pacing.maximumWpm = pacing.safeMaximumWpm = rsvp::RsvpSpeedProbe::MAXIMUM_WPM;
  pacing.paceStepWpm = rsvp::RsvpSpeedProbe::STEP_WPM;
#endif
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  // The X3 diagnostic measures the display path at its maximum fixture rate.
  // UINT16_MAX is clamped by RsvpSession but baseIntervalMs() becomes zero,
  // so no artificial 300 WPM wait hides sub-200 ms window updates.
  pacing.paceWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  pacing.minimumWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  pacing.maximumWpm = pacing.safeMaximumWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  pacing.paceStepWpm = 1;
  windowReturnAnchor = initialAnchor;
#endif
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  windowPacing = pacing;
#endif
  const bool groupingEnabled =
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
      false;
#else
      prepareGroupingFonts();
#endif
  const auto storedGroupingChoice =
      static_cast<rsvp::GroupingLanguageChoice>(static_cast<uint8_t>(epub->getGroupingLanguagePreference().load()));
  const auto groupingLanguage = rsvp::resolveGroupingLanguage(storedGroupingChoice, epub->getLanguage());
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  session = makeUniqueNoThrow<rsvp::RsvpSession>(windowFixtureSource, rsvp::ResumeAnchor{}, pacing, groupingEnabled,
                                                 &RsvpReaderActivity::fitPresentationGroup, this, groupingLanguage);
  checkpointWritesDisabled = true;
#else
  session = makeUniqueNoThrow<rsvp::RsvpSession>(*source, initialAnchor, pacing, groupingEnabled,
                                                 &RsvpReaderActivity::fitPresentationGroup, this, groupingLanguage);
#endif
  if (!session) {
    LOG_ERR("RSVP", "Failed to allocate session");
    return enterFatalFallback(rsvp::Error::SourceOpen);
  }
#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  // The production line path is opt-in per the runtime-selected driver. A
  // first checked full frame establishes the driver's differential baseline;
  // unsupported controllers remain on the existing full-frame path.
  renderer.setExperimentalWindowUpdates(true);
  productionWindowEnabled = renderer.supportsExperimentalWindowUpdates();
  renderer.invalidateWindowBaseline();
  productionDisplayFailure.store(false, std::memory_order_release);
  productionDisplayRecoveryRequested.store(false, std::memory_order_release);
  productionStatusValid = false;
  presentedRegionValid = false;
#endif
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
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  LOG_INF("RSVP-SPEED", "config version=%s cap=%u grouping=%u clause=%u sentence=%u paragraph=%u", CROSSPOINT_VERSION,
          rsvp::RsvpSpeedProbe::MAXIMUM_WPM, groupingEnabled ? 1u : 0u, pacing.clausePausePercent,
          pacing.sentencePausePercent, pacing.paragraphPausePercent);
#endif

#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  if (restoredFromCheckpoint) session->restoreAfterCheckpoint(restoredTokenHash32, restoredTokenLength);
#endif
  currentDecision = session->step({.nowMs = millis()});
#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  if (restoredFromCheckpoint && !session->checkpointIdentityValidated()) {
    LOG_INF("RSVP", "Checkpoint token identity no longer resolves; returning to Paged progress");
    currentDecision = {};
    switchToPagedPending = true;
    switchToNativeProgress = true;
    invalidateCheckpointOnNativeFallback = true;
    return true;
  }
#endif
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
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  pacing.paceWpm = currentDecision.paceWpm;
  pacing.minimumWpm = rsvp::RsvpSpeedProbe::MINIMUM_WPM;
  pacing.maximumWpm = pacing.safeMaximumWpm = rsvp::RsvpSpeedProbe::MAXIMUM_WPM;
  pacing.paceStepWpm = rsvp::RsvpSpeedProbe::STEP_WPM;
#endif
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  pacing = windowPacing;
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
#else
  // Settings can change orientation, font metrics, grouping, guides, and
  // therefore the logical line geometry. Require a checked full baseline
  // before resuming the window candidate.
  renderer.setExperimentalWindowUpdates(true);
  productionWindowEnabled = renderer.supportsExperimentalWindowUpdates();
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
  productionStatusValid = false;
#endif
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
  if (session) {
    const auto storedGroupingChoice =
        static_cast<rsvp::GroupingLanguageChoice>(static_cast<uint8_t>(epub->getGroupingLanguagePreference().load()));
    const auto groupingLanguage = rsvp::resolveGroupingLanguage(storedGroupingChoice, epub->getLanguage());
    applyDecision(session->configureWhilePaused(pacing,
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
                                                false,
#else
                                                prepareGroupingFonts(),
#endif
                                                groupingLanguage));
  }
  if (controlPanel) controlPanel->begin();
  currentDecision.render = true;
  requestUpdate();
}

void RsvpReaderActivity::openSettings() {
  auto settings = makeUniqueNoThrow<SettingsActivity>(renderer, mappedInput, true, epub->getCachePath());
  if (!settings) {
    LOG_ERR("RSVP", "OOM: settings activity");
    return;
  }
  onSystemModalOpening();
  startActivityForResult(std::move(settings), [this](const ActivityResult&) { applySettings(); });
}

#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
bool RsvpReaderActivity::restartWindowDiagnostic(const uint32_t nowMs) {
  const bool candidateEnabled = windowProbeWindowUpdateRequested || windowBenchmark.windowCandidateEnabled() ||
                                windowBenchmark.tightCandidateEnabled();
  renderer.setExperimentalWindowUpdates(candidateEnabled);
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
  drawnRegionValid = false;
  tightRegionValid = false;
  windowHeaderCpuMhz = observedCpuMhz();
  // The tight optical probe is intentionally tied to the fixture's known
  // `to` entry: its full resync shows the preceding `horizon`, then the one
  // queued StepForward presents `to` in the actual tight window update.
  const rsvp::ResumeAnchor fixtureAnchor =
      windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe
          ? rsvp::ResumeAnchor{.spineIndex = 0, .visibleTextOffset = 2, .sameOffsetOrdinal = 0, .valid = true}
          : rsvp::ResumeAnchor{};
  session.reset();
  session = makeUniqueNoThrow<rsvp::RsvpSession>(windowFixtureSource, fixtureAnchor, windowPacing, false,
                                                 &RsvpReaderActivity::fitPresentationGroup, this,
                                                 rsvp::GroupingLanguage::English);
  if (!session) {
    windowBenchmark.fail(nowMs);
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
    windowDiagnosticRunActive.store(false, std::memory_order_release);
    windowReportPending = true;
    return false;
  }
  currentDecision = session->step({.nowMs = nowMs});
  windowAutoPlayPending = windowBenchmark.running();
  panelVisible = false;
  currentDecision.render = true;
  requestUpdate();
  return true;
}

bool RsvpReaderActivity::queueWindowProbeFrame(const uint32_t nowMs) {
  if (!session) return false;
  const auto decision = session->step({.nowMs = nowMs, .action = rsvp::Action::StepForward});
  if (!decision.render) {
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
    windowDiagnosticRunActive.store(false, std::memory_order_release);
    windowProbePresentationPending = false;
    windowProbeAwaitingConfirm = false;
    windowProbeConfirmArmed = false;
    windowProbeFreshConfirmRequired = false;
    windowProbeWindowUpdateRequested = false;
    windowBenchmark.fail(nowMs);
    windowReportPending = true;
    currentDecision = decision;
    currentDecision.render = true;
    requestUpdate();
    return false;
  }
  windowProbeWindowUpdateRequested = true;
  windowProbePresentationPending = true;
  windowProbeAwaitingConfirm = false;
  windowProbeConfirmArmed = false;
  windowProbeFreshConfirmRequired = false;
  renderer.setExperimentalWindowUpdates(true);
  applyDecision(decision);
  return true;
}

void RsvpReaderActivity::startWindowDiagnostic(const uint32_t nowMs) {
  windowDiagnosticStarted = true;
  windowReportPending = false;
  windowReportSaved = false;
  windowReportFailed = false;
  windowDiagnosticState.reset();
  windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::FullSizePtl;
  windowProbePresentationPending = false;
  windowProbeAwaitingConfirm = false;
  windowProbeConfirmArmed = false;
  windowProbeFreshConfirmRequired = false;
  windowProbeWindowUpdateRequested = false;
  windowProbeAdvanceRequested = false;
  windowFailureAwaitingUser = false;
  windowFailureConfirmArmed = false;
  windowFailureRecoveryRequested = false;
  windowFullProbeConfirmed = false;
  windowLineProbeConfirmed = false;
  windowTightProbeConfirmed = false;
  checkedDisplayFailure.store(false, std::memory_order_release);
  windowDiagnosticRunActive.store(true, std::memory_order_release);
  windowBenchmark.reset();
  restartWindowDiagnostic(nowMs);
}

void RsvpReaderActivity::updateWindowDiagnostic(const uint32_t nowMs) {
  if (windowBenchmark.currentPhase() == rsvp::WindowBenchmarkPhase::Complete) {
    finishWindowDiagnostic(false, nowMs);
    return;
  }
  switch (windowBenchmark.currentPhase()) {
    case rsvp::WindowBenchmarkPhase::FullWarmup:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::FullWarmup;
      break;
    case rsvp::WindowBenchmarkPhase::FullMeasurement:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::FullMeasurement;
      break;
    case rsvp::WindowBenchmarkPhase::WindowWarmup:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::WindowWarmup;
      break;
    case rsvp::WindowBenchmarkPhase::WindowMeasurement:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::WindowMeasurement;
      break;
    case rsvp::WindowBenchmarkPhase::TightProbe:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::TightProbe;
      break;
    case rsvp::WindowBenchmarkPhase::TightWarmup:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::TightWarmup;
      break;
    case rsvp::WindowBenchmarkPhase::TightMeasurement:
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::TightMeasurement;
      break;
    default:
      break;
  }
  windowProbeWindowUpdateRequested = false;
  windowProbePresentationPending = false;
  windowProbeAwaitingConfirm = false;
  windowProbeConfirmArmed = false;
  windowProbeFreshConfirmRequired = false;
  restartWindowDiagnostic(nowMs);
}

void RsvpReaderActivity::advanceWindowProbe(const uint32_t nowMs) {
  if (!windowProbeAwaitingConfirm || windowFailureAwaitingUser) return;
  const bool preservingTightBaseline = windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe;
  windowProbeAdvanceRequested = false;
  windowProbeAwaitingConfirm = false;
  windowProbeConfirmArmed = false;
  windowProbeFreshConfirmRequired = false;
  windowProbePresentationPending = true;
  windowProbeWindowUpdateRequested = false;
  renderer.setExperimentalWindowUpdates(false);
  if (!preservingTightBaseline) {
    renderer.invalidateWindowBaseline();
    presentedRegionValid = false;
  }

  if (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl) {
    // The full-size PTL image is accepted first; only a confirmed visual
    // result may move the fixture to the line candidate.
    windowFullProbeConfirmed = true;
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::LineCandidate;
    if (!session) {
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
      windowDiagnosticRunActive.store(false, std::memory_order_release);
      windowProbePresentationPending = false;
      windowBenchmark.fail(nowMs);
      windowReportPending = true;
      currentDecision = {};
      currentDecision.render = true;
      requestUpdate();
      return;
    }
    const auto decision = session->step({.nowMs = nowMs, .action = rsvp::Action::StepForward});
    if (!decision.render) {
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
      windowDiagnosticRunActive.store(false, std::memory_order_release);
      windowReportPending = true;
      currentDecision = decision;
      currentDecision.render = true;
      requestUpdate();
      return;
    }
    applyDecision(decision);
    return;
  }

  if (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::LineCandidate) {
    // The second confirmation starts the existing bounded A/B run. Its first
    // frame is a new full-size baseline before the candidate window branch.
    windowLineProbeConfirmed = true;
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::FullWarmup;
    windowProbePresentationPending = false;
    windowBenchmark.start(nowMs);
    restartWindowDiagnostic(nowMs);
    return;
  }

  if (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe) {
    // The tight candidate is opt-in as well. Its first checked presentation is
    // a full resync; only the following confirmed update may enter the tight
    // measurement phases.
    windowTightProbeConfirmed = true;
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::TightWarmup;
    windowProbePresentationPending = false;
    windowBenchmark.startTight(nowMs);
    restartWindowDiagnostic(nowMs);
  }
}

void RsvpReaderActivity::finishWindowDiagnostic(const bool aborted, const uint32_t nowMs) {
  if (aborted && windowBenchmark.running()) windowBenchmark.abort(nowMs);
  windowDiagnosticPhase = aborted ? rsvp::WindowDiagnosticPhase::Aborted : rsvp::WindowDiagnosticPhase::Complete;
  windowDiagnosticRunActive.store(false, std::memory_order_release);
  windowProbePresentationPending = false;
  windowProbeAwaitingConfirm = false;
  windowProbeConfirmArmed = false;
  windowProbeFreshConfirmRequired = false;
  windowProbeWindowUpdateRequested = false;
  windowProbeAdvanceRequested = false;
  windowFailureAwaitingUser = false;
  windowFailureConfirmArmed = false;
  windowFailureRecoveryRequested = false;
  renderer.setExperimentalWindowUpdates(false);
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
  windowAutoPlayPending = false;
  if (session && currentDecision.state == rsvp::State::Playing) {
    applyDecision(session->step({.nowMs = nowMs, .action = rsvp::Action::TogglePlayback}));
  }
  panelVisible = false;
  windowReportPending = true;
  currentDecision.render = true;
  requestUpdate();
}

bool RsvpReaderActivity::writeWindowDiagnosticReport() {
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  HalFile file;
  if (!Storage.openFileForWrite("RSVP-WINDOW", WINDOW_REPORT_PATH, file)) return false;
  const auto detection = renderer.controllerDetection();
  const char* diagnosticStatus = windowFailureAwaitingUser                                        ? "awaiting_recovery"
                                 : windowProbeAwaitingConfirm                                     ? "awaiting_confirm"
                                 : windowDiagnosticRunActive.load(std::memory_order_acquire)      ? "running"
                                 : windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Complete ? "complete"
                                 : windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Aborted  ? "aborted"
                                                                                                  : "stopped";
  HalFileReportSink sink(file);
  const bool wrote =
      rsvp::writeRsvpWindowReport(sink, windowBenchmark,
                                  {.controller = controllerName(detection.controller),
                                   .confidence = confidenceName(detection.confidence),
                                   .orientation = orientationName(renderer.getOrientation()),
                                   .power = "not_sampled",
                                   .targetWpm = rsvp::RsvpWindowBenchmark::REPORT_TARGET_WPM,
                                   .firmwareVersion = CROSSPOINT_VERSION,
                                   .diagnosticStage = rsvp::windowDiagnosticPhaseName(windowDiagnosticPhase),
                                   .diagnosticStatus = diagnosticStatus,
                                   .fullProbeConfirmed = windowFullProbeConfirmed,
                                   .lineProbeConfirmed = windowLineProbeConfirmed,
                                   .tightProbeConfirmed = windowTightProbeConfirmed},
                                  windowDiagnosticState);
  file.flush();
  return file.close() && wrote;
}

#endif

void RsvpReaderActivity::setDrawnRegion(const int left, const int top, const int right, const int bottom) {
  drawnRegion = {left, top, right - left, bottom - top};
  drawnRegionValid = drawnRegion.width > 0 && drawnRegion.height > 0;
}

void RsvpReaderActivity::loop() {
  if (fatalFallbackReady.exchange(false)) {
    switchToPagedPending = true;
    switchToPaged();
    return;
  }

#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  {
    RenderLock diagnosticLock(false);
    if (diagnosticLock.ownsLock()) {
      if (windowAbortRequested && windowDiagnosticStarted) {
        windowAbortRequested = false;
        finishWindowDiagnostic(true, millis());
        return;
      }
      const bool canStartWindowDiagnostic = windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Ready ||
                                            windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Complete ||
                                            windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Aborted ||
                                            windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Error;
      if (windowStartRequested && canStartWindowDiagnostic && !windowFailureAwaitingUser &&
          !checkedDisplayFailure.load(std::memory_order_acquire)) {
        windowStartRequested = false;
        startWindowDiagnostic(millis());
        return;
      }
      if (windowProbeAdvanceRequested && windowProbeAwaitingConfirm && !windowFailureAwaitingUser) {
        windowProbeAdvanceRequested = false;
        advanceWindowProbe(millis());
        return;
      }
      const bool reportMayWrite =
          !windowFailureAwaitingUser ||
          (checkedDisplayFailure.load(std::memory_order_acquire) && renderer.checkedDisplayReady());
      if (windowReportPending && reportMayWrite) {
        windowReportPending = false;
        windowReportSaved = writeWindowDiagnosticReport();
        windowReportFailed = !windowReportSaved;
        if (!windowFailureAwaitingUser) {
          currentDecision.render = true;
          requestUpdate();
        }
      }
      if (checkedDisplayFailure.load(std::memory_order_acquire) && windowFailureRecoveryRequested &&
          renderer.checkedDisplayReady()) {
        currentDecision.render = true;
        requestUpdate();
      }
      if (windowBenchmark.running() && windowBenchmark.update(millis())) {
        updateWindowDiagnostic(millis());
        return;
      }
    }
  }
  if (windowSettingsRequested && !checkedDisplayFailure.load(std::memory_order_acquire)) {
    windowSettingsRequested = false;
    openSettings();
    return;
  }
#endif
  if (switchToPagedPending
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
      && !checkedDisplayFailure.load(std::memory_order_acquire)
#endif
  ) {
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
  if (controlPanel && panelVisible && !productionDisplayFailure.load(std::memory_order_acquire) &&
      !swallowTouchRelease && !swallowedTouchRelease && !pauseTouchPending) {
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
#if !defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS) && \
    (!defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC)
          SETTINGS.rsvpPaceWpm = currentDecision.paceWpm;
#endif
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
          if (checkedDisplayFailure.load(std::memory_order_acquire)) {
            windowSettingsRequested = true;
            return;
          }
#endif
          openSettings();
        }
          return;
        case RsvpControlPanelUi::Event::None:
          break;
      }
      if (action != rsvp::Action::None && !pendingActions.push(action)) LOG_ERR("RSVP", "Pending action buffer full");
      return;
    }
  }
  if (controlPanel && currentDecision.state == rsvp::State::Playing &&
      !productionDisplayFailure.load(std::memory_order_acquire)) {
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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  const bool windowConfirmLongPressed =
      currentDecision.state != rsvp::State::Playing &&
      mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, ReaderUtils::SKIP_HOLD_MS);
  if (windowConfirmLongPressed &&
      (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Ready ||
       windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Complete ||
       windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Aborted ||
       windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Error) &&
      !checkedDisplayFailure.load(std::memory_order_acquire)) {
    windowStartRequested = true;
    return;
  }
  // A probe can finish while Confirm is still held. Require the button to be
  // observed up after the successful update before accepting a new
  // press/release pair, so the start hold or a press during rendering can
  // never advance the next probe.
  if (windowProbeAwaitingConfirm && windowProbeFreshConfirmRequired &&
      !mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
    mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    windowProbeFreshConfirmRequired = false;
  }
  const bool freshProbeInput = !windowProbeFreshConfirmRequired;
  const bool windowConfirmPressed = freshProbeInput && mappedInput.wasPressed(MappedInputManager::Button::Confirm);
  if (windowFailureAwaitingUser && windowConfirmPressed) windowFailureConfirmArmed = true;
  if (windowProbeAwaitingConfirm && windowConfirmPressed) windowProbeConfirmArmed = true;
  const bool windowConfirmReleased = freshProbeInput && mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  if (windowConfirmReleased && checkedDisplayFailure.load(std::memory_order_acquire) && windowFailureAwaitingUser &&
      windowFailureConfirmArmed) {
    windowFailureConfirmArmed = false;
    windowFailureRecoveryRequested = true;
    return;
  }
  if (windowConfirmReleased && windowProbeAwaitingConfirm && windowProbeConfirmArmed) {
    windowProbeConfirmArmed = false;
    windowProbeAdvanceRequested = true;
    return;
  }
  if (windowConfirmReleased && windowBenchmark.running()) {
    windowAbortRequested = true;
    return;
  }
#endif
  const bool backLongPressed =
      mappedInput.wasLongPressed(MappedInputManager::Button::Back, ReaderUtils::GO_BACK_OR_HOME_MS);
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  if (backLongPressed && windowDiagnosticStarted && windowFailureAwaitingUser) {
    // A held Back is still a recovery request while a checked display failure
    // owns the activity; never queue Exit until that latch has cleared.
    windowFailureConfirmArmed = false;
    windowFailureRecoveryRequested = true;
    return;
  }
  const bool diagnosticActive = windowDiagnosticStarted &&
                                windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Ready &&
                                windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Complete &&
                                windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Aborted &&
                                windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Error;
  if (backLongPressed && diagnosticActive && !windowFailureAwaitingUser) {
    // Finish through the same locked abort path as a short Back release. This
    // prevents a held Back from queuing Exit and bypassing report generation.
    windowAbortRequested = true;
    return;
  }
#endif
  if (backLongPressed) {
    if (!pendingActions.push(rsvp::Action::Exit)) LOG_ERR("RSVP", "Pending action buffer full");
  } else {
    const auto queueInputAction = [this](const bool triggered, const rsvp::Action action) {
      if (triggered && !pendingActions.push(action)) LOG_ERR("RSVP", "Pending action buffer full");
    };
    const bool backReleased = mappedInput.wasReleased(MappedInputManager::Button::Back);
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
    if (backReleased && windowDiagnosticStarted && windowFailureAwaitingUser) {
      windowFailureConfirmArmed = false;
      windowFailureRecoveryRequested = true;
      return;
    }
    if (backReleased && windowDiagnosticStarted &&
        (windowProbeAwaitingConfirm || windowProbePresentationPending || windowBenchmark.running())) {
      windowProbeConfirmArmed = false;
      windowAbortRequested = true;
      return;
    }
#endif
    queueInputAction(backReleased, rsvp::Action::ModeSwitch);
#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Confirm), rsvp::Action::TogglePlayback);
#endif
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Left), rsvp::Action::PaceDown);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::Right), rsvp::Action::PaceUp);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::PageBack), rsvp::Action::RewindFive);
    queueInputAction(mappedInput.wasReleased(MappedInputManager::Button::PageForward), rsvp::Action::StepForward);
  }

#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  // Inputs above remain queued while a failed checked update owns an
  // unverified controller. Activity transitions and modal redraws resume only
  // after the locked readiness path clears this latch.
  if (checkedDisplayFailure.load(std::memory_order_acquire)) return;
#else
  // Keep all input queued while a checked presentation is awaiting recovery.
  // Readiness only schedules the same pending frame for a checked full
  // resync; the latch is cleared by renderBook after that resync succeeds.
  if (productionDisplayFailure.load(std::memory_order_acquire)) {
    if (!productionDisplayRecoveryRequested.load(std::memory_order_acquire) && renderer.checkedDisplayReady()) {
      productionDisplayRecoveryRequested.store(true, std::memory_order_release);
      currentDecision.render = true;
      requestUpdate();
    }
    return;
  }
#endif

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
#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
    if (decision.checkpointRequested && !saveCheckpoint()) {
      LOG_ERR("RSVP", "Failed to save RSVP checkpoint");
    }
#endif
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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  renderer.setExperimentalWindowUpdates(false);
  renderer.invalidateWindowBaseline();
  activityManager.goToReader(
      bookPath, false,
      ReaderLaunchContext{ReaderLaunchMode::Paged, windowReturnAnchor, false, 0, 0, false, false, bookRevision});
  return;
#else
  renderer.setExperimentalWindowUpdates(false);
  renderer.invalidateWindowBaseline();
  productionWindowEnabled = false;
  presentedRegionValid = false;
  productionStatusValid = false;
#endif
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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  renderer.setExperimentalWindowUpdates(false);
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
#else
  renderer.setExperimentalWindowUpdates(false);
  renderer.invalidateWindowBaseline();
  productionWindowEnabled = false;
  presentedRegionValid = false;
  productionStatusValid = false;
#endif
  if (currentDecision.state == rsvp::State::Finished) {
    if (!finalizeCompletedBook()) LOG_ERR("RSVP", "Failed to finalize completed book on exit");
  } else if (!checkpointWritesDisabled) {
    saveCheckpoint();
  }
  ReaderActivity::onExit();
}

void RsvpReaderActivity::applyDecision(const rsvp::Decision& decision) {
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  if (decision.state != currentDecision.state || decision.paceWpm != currentDecision.paceWpm) {
    speedProbe.interrupt();
  }
#endif
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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
#else
  renderer.invalidateWindowBaseline();
  presentedRegionValid = false;
  productionStatusValid = false;
#endif
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
  drawnRegionValid = false;
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  tightRegionValid = false;
#endif
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
  // Advances do not bound arbitrary SD-font ink overhangs. Use the complete
  // RSVP line width and only narrow vertically; this remains bounded while
  // guaranteeing that old and new companion/active glyph tails are covered.
  int regionLeft = groupLayoutInput.leftBound;
  int regionRight = groupLayoutInput.rightBound;
  int regionTop = y;
  int regionBottom = y + lineHeight;
  if (group) {
    for (uint8_t index = 0; index < group->count; ++index) {
      if (index == group->activeIndex) continue;
      renderer.drawText(companionFontId, groupLayout.positions[index], companionY, group->tokens[index].text, true,
                        EpdFontFamily::REGULAR);
      regionTop = std::min(regionTop, companionY);
      regionBottom = std::max(regionBottom, companionY + renderer.getLineHeight(companionFontId));
    }
  }
  renderer.drawText(activeFontId, groupLayout.active.prefixX, y, prefixBuffer, true, EpdFontFamily::REGULAR);
  renderer.drawText(activeFontId, groupLayout.active.pivotX, y, pivotBuffer, true, EpdFontFamily::BOLD);
  renderer.drawText(activeFontId, groupLayout.active.suffixX, y, suffixBuffer, true, EpdFontFamily::REGULAR);

  if (SETTINGS.rsvpGuideStyle != CrossPointSettings::RSVP_GUIDES_OFF) {
    const int focusX = groupLayoutInput.focusX;
    const int upperTop = std::max(marginTop, y - 22);
    const int upperBottom = std::max(marginTop, y - 8);
    const int lowerTop = std::min(usableBottom - 1, y + lineHeight + 8);
    const int lowerBottom = std::min(usableBottom - 1, y + lineHeight + 22);
    renderer.drawLine(focusX, upperTop, focusX, upperBottom, 2, true);
    renderer.drawLine(focusX, lowerTop, focusX, lowerBottom, 2, true);
    regionTop = std::min(regionTop, upperTop);
    regionBottom = std::max(regionBottom, lowerBottom + 1);
  }
  static constexpr int REGION_PADDING = 12;
  setDrawnRegion(regionLeft - REGION_PADDING, regionTop - REGION_PADDING, regionRight + REGION_PADDING,
                 regionBottom + REGION_PADDING);
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  const bool tightStage = windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe ||
                          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightWarmup ||
                          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightMeasurement;
  if (tightStage && drawnRegionValid) {
    tightRegion = renderer.inkBounds(drawnRegion);
    tightRegionValid = tightRegion.width > 0 && tightRegion.height > 0;
  }
#endif
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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  char status[128];
  const char* phaseText = tr(STR_RSVP_WINDOW_READY);
  switch (windowDiagnosticPhase) {
    case rsvp::WindowDiagnosticPhase::FullSizePtl:
      phaseText = tr(STR_RSVP_WINDOW_PROBE_FULL);
      break;
    case rsvp::WindowDiagnosticPhase::LineCandidate:
      phaseText = tr(STR_RSVP_WINDOW_PROBE_LINE);
      break;
    case rsvp::WindowDiagnosticPhase::FullWarmup:
      phaseText = tr(STR_RSVP_WINDOW_FULL_WARMUP);
      break;
    case rsvp::WindowDiagnosticPhase::FullMeasurement:
      phaseText = tr(STR_RSVP_WINDOW_FULL_RUN);
      break;
    case rsvp::WindowDiagnosticPhase::WindowWarmup:
      phaseText = tr(STR_RSVP_WINDOW_PARTIAL_WARMUP);
      break;
    case rsvp::WindowDiagnosticPhase::WindowMeasurement:
      phaseText = tr(STR_RSVP_WINDOW_PARTIAL_RUN);
      break;
    case rsvp::WindowDiagnosticPhase::TightProbe:
      phaseText = tr(STR_RSVP_WINDOW_TIGHT_PROBE);
      break;
    case rsvp::WindowDiagnosticPhase::TightWarmup:
      phaseText = tr(STR_RSVP_WINDOW_TIGHT_WARMUP);
      break;
    case rsvp::WindowDiagnosticPhase::TightMeasurement:
      phaseText = tr(STR_RSVP_WINDOW_TIGHT_RUN);
      break;
    case rsvp::WindowDiagnosticPhase::Complete:
      phaseText = tr(STR_RSVP_WINDOW_COMPLETE);
      break;
    case rsvp::WindowDiagnosticPhase::Aborted:
      phaseText = tr(STR_RSVP_WINDOW_ABORTED);
      break;
    case rsvp::WindowDiagnosticPhase::Error:
      phaseText = tr(STR_RSVP_WINDOW_ERROR);
      break;
    case rsvp::WindowDiagnosticPhase::Ready:
      break;
  }
  renderer.drawCenteredText(SMALL_FONT_ID, 12, phaseText);
  const auto detection = renderer.controllerDetection();
  const uint16_t displayedCpuMhz = windowBenchmark.running() ? windowHeaderCpuMhz : observedCpuMhz();
  if (displayedCpuMhz == 0) {
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_CONTROLLER_NO_CPU), controllerName(detection.controller),
             confidenceDisplayName(detection.confidence));
  } else {
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_CONTROLLER), controllerName(detection.controller),
             confidenceDisplayName(detection.confidence), static_cast<unsigned>(displayedCpuMhz));
  }
  renderer.drawCenteredText(SMALL_FONT_ID, 15 + renderer.getLineHeight(SMALL_FONT_ID), status);

  if (!windowBenchmark.running() && windowBenchmark.currentPhase() != rsvp::WindowBenchmarkPhase::Idle) {
    const auto& full = windowBenchmark.fullStats();
    const auto& window = windowBenchmark.windowStats();
    const auto& tight = windowBenchmark.tightStats();
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_RESULT_FULL), static_cast<unsigned long>(full.frame.count),
             static_cast<unsigned long>(full.frame.minimumMs), static_cast<unsigned long>(full.frame.meanMs()),
             static_cast<unsigned long>(full.frame.maximumMs), static_cast<unsigned long>(full.framesPerMinute()));
    renderer.drawCenteredText(SMALL_FONT_ID, 18 + renderer.getLineHeight(SMALL_FONT_ID) * 2, status);
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_RESULT_PARTIAL), static_cast<unsigned long>(window.frame.count),
             static_cast<unsigned long>(window.frame.minimumMs), static_cast<unsigned long>(window.frame.meanMs()),
             static_cast<unsigned long>(window.frame.maximumMs), static_cast<unsigned long>(window.framesPerMinute()));
    renderer.drawCenteredText(SMALL_FONT_ID, 21 + renderer.getLineHeight(SMALL_FONT_ID) * 3, status);
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_RESULT_TIGHT), static_cast<unsigned long>(tight.frame.count),
             static_cast<unsigned long>(tight.frame.minimumMs), static_cast<unsigned long>(tight.frame.meanMs()),
             static_cast<unsigned long>(tight.frame.maximumMs), static_cast<unsigned long>(tight.framesPerMinute()));
    renderer.drawCenteredText(SMALL_FONT_ID, 24 + renderer.getLineHeight(SMALL_FONT_ID) * 4, status);
    const auto fallback = tight.fallbackCount != 0    ? tight.lastFallback
                          : window.fallbackCount != 0 ? window.lastFallback
                                                      : full.lastFallback;
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_RESULT_ACTUAL),
             static_cast<unsigned long>(full.windowCount + window.windowCount),
             static_cast<unsigned long>(full.fullCount + window.fullCount + tight.fullCount),
             static_cast<unsigned long>(full.cleanupCount + window.cleanupCount + tight.cleanupCount),
             static_cast<unsigned long>(full.fallbackCount + window.fallbackCount + tight.fallbackCount),
             fallbackDisplayName(fallback));
    renderer.drawCenteredText(SMALL_FONT_ID, 27 + renderer.getLineHeight(SMALL_FONT_ID) * 5, status);
    if (full.cpuSampleCount == 0 && window.cpuSampleCount == 0 && tight.cpuSampleCount == 0) {
      snprintf(status, sizeof(status), "%s", tr(STR_RSVP_WINDOW_RESULT_CPU_UNAVAILABLE));
    } else {
      snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_RESULT_CPU), static_cast<unsigned>(full.minimumCpuMhz),
               static_cast<unsigned>(full.maximumCpuMhz), static_cast<unsigned>(window.minimumCpuMhz),
               static_cast<unsigned>(window.maximumCpuMhz));
    }
    renderer.drawCenteredText(SMALL_FONT_ID, 30 + renderer.getLineHeight(SMALL_FONT_ID) * 6, status);
  }
  if (windowDiagnosticState.lastFailure.valid) {
    const auto& failure = windowDiagnosticState.lastFailure;
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_FAILURE_DETAIL), rsvp::windowDiagnosticPhaseName(failure.phase),
             rsvp::windowDiagnosticOperationName(failure.operation), rsvp::windowActualRefreshName(failure.requested),
             rsvp::windowActualRefreshName(failure.actual), static_cast<unsigned>(failure.rawError),
             static_cast<unsigned long>(failure.durationMs));
    renderer.drawCenteredText(SMALL_FONT_ID, 33 + renderer.getLineHeight(SMALL_FONT_ID) * 7, status);
    snprintf(status, sizeof(status), tr(STR_RSVP_WINDOW_LAST_SUCCESS),
             rsvp::windowDiagnosticOperationName(failure.lastSuccessfulOperation));
    renderer.drawCenteredText(SMALL_FONT_ID, 36 + renderer.getLineHeight(SMALL_FONT_ID) * 8, status);
  }
  const bool windowProbePhase = windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl ||
                                windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::LineCandidate ||
                                windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe;
  const char* hint = windowFailureAwaitingUser ? tr(STR_RSVP_WINDOW_FAILURE_CONFIRM)
                     : (windowProbePhase || windowProbeAwaitingConfirm || windowProbePresentationPending)
                         ? tr(STR_RSVP_WINDOW_PROBE_CONFIRM)
                     : windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::Ready ? tr(STR_RSVP_WINDOW_START_HINT)
                     : windowReportFailed                                          ? tr(STR_RSVP_WINDOW_CSV_ERROR)
                     : windowReportSaved                                           ? tr(STR_RSVP_WINDOW_CSV_SAVED)
                                                                                   : tr(STR_RSVP_WINDOW_PROBE_RUNNING);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - renderer.getLineHeight(SMALL_FONT_ID) - 12,
                            hint);
#else
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  char status[128];
#else
  char status[48];
#endif
  const char* stateText = currentDecision.state == rsvp::State::Playing ? tr(STR_RSVP_PLAYING) : tr(STR_RSVP_PAUSED);
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  snprintf(status, sizeof(status), tr(STR_RSVP_SPEED_MODE), stateText, static_cast<unsigned>(currentDecision.paceWpm));
#else
  snprintf(status, sizeof(status), "%s  %u", stateText, static_cast<unsigned>(currentDecision.paceWpm));
#endif
  renderer.drawCenteredText(SMALL_FONT_ID, 12, status);
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
  snprintf(status, sizeof(status), tr(STR_RSVP_SPEED_METRICS),
           static_cast<unsigned>(speedProbe.actualFramesPerMinute()),
           static_cast<unsigned long>(speedProbe.averageFastMs()));
  renderer.drawCenteredText(SMALL_FONT_ID, 15 + renderer.getLineHeight(SMALL_FONT_ID), status);
#endif
  const char* hint = isSkippableNonTextPause(currentDecision.pauseReason) ? tr(STR_RSVP_HINT_BOUNDARY_SKIP)
                                                                          : tr(STR_RSVP_HINT_MODE_SWITCH);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - renderer.getLineHeight(SMALL_FONT_ID) - 12,
                            hint);
#endif
}

void RsvpReaderActivity::renderBook() {
  if (!currentDecision.render && pauseMessage() == nullptr) return;
#if defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS) || \
    (defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC)
  const uint32_t frameStartedAt = millis();
#endif
  drawnRegionValid = false;
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  if (checkedDisplayFailure.load(std::memory_order_acquire) &&
      (!windowFailureRecoveryRequested || !renderer.checkedDisplayReady())) {
    return;
  }
#else
  if (productionDisplayFailure.load(std::memory_order_acquire) &&
      !productionDisplayRecoveryRequested.load(std::memory_order_acquire)) {
    return;
  }
#endif

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
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  HalDisplay::DisplayUpdateResult updateResult;
  const bool tightStage = windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe ||
                          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightWarmup ||
                          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightMeasurement;
  const bool tightCandidateInvalid = tightStage && message == nullptr && drawnRegionValid && !tightRegionValid;
  const bool probeWindow = windowDiagnosticStarted && windowProbeWindowUpdateRequested && !cleanup &&
                           message == nullptr && windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Ready &&
                           windowDiagnosticPhase != rsvp::WindowDiagnosticPhase::Error;
  const bool sustainedWindow = windowDiagnosticStarted && windowBenchmark.windowCandidateEnabled() && !cleanup &&
                               message == nullptr && currentDecision.state == rsvp::State::Playing;
  const bool sustainedTightWindow = windowDiagnosticStarted && windowBenchmark.tightCandidateEnabled() && !cleanup &&
                                    message == nullptr && currentDecision.state == rsvp::State::Playing;
  const bool useWindow = (probeWindow || sustainedWindow || sustainedTightWindow) && drawnRegionValid &&
                         !tightCandidateInvalid && presentedRegionValid &&
                         renderer.windowBaselineState() == HalDisplay::WindowBaselineState::Valid;
  GfxRenderer::LogicalRegion requestedWindowRegion;
  const auto operation = checkedDisplayFailure.load(std::memory_order_acquire) && windowFailureRecoveryRequested
                             ? rsvp::WindowDiagnosticOperation::FullRecovery
                         : useWindow
                             ? (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe
                                    ? rsvp::WindowDiagnosticOperation::TightProbe
                                : tightStage ? rsvp::WindowDiagnosticOperation::TightWindow
                                : windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl
                                    ? rsvp::WindowDiagnosticOperation::FullSizePtl
                                    : rsvp::WindowDiagnosticOperation::LineCandidate)
                             : (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl ||
                                        windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::LineCandidate ||
                                        windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe ||
                                        windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightWarmup ||
                                        windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightMeasurement ||
                                        windowBenchmark.currentPhase() == rsvp::WindowBenchmarkPhase::FullWarmup ||
                                        windowBenchmark.currentPhase() == rsvp::WindowBenchmarkPhase::FullMeasurement ||
                                        windowBenchmark.currentPhase() == rsvp::WindowBenchmarkPhase::WindowWarmup ||
                                        windowBenchmark.currentPhase() == rsvp::WindowBenchmarkPhase::WindowMeasurement
                                    ? rsvp::WindowDiagnosticOperation::FullResync
                                    : rsvp::WindowDiagnosticOperation::FullFrame);
  if (useWindow) {
    if (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl) {
      requestedWindowRegion = {0, 0, renderer.getScreenWidth(), renderer.getScreenHeight()};
      updateResult = renderer.displayWindowChecked(requestedWindowRegion);
    } else if (tightStage) {
      requestedWindowRegion = unionRegions(presentedRegion, tightRegion);
      updateResult = renderer.displayWindowChecked(requestedWindowRegion);
    } else {
      requestedWindowRegion = unionRegions(presentedRegion, drawnRegion);
      updateResult = renderer.displayWindowChecked(requestedWindowRegion);
    }
  } else {
    updateResult = renderer.displayBufferChecked(mode);
  }
#else
  const uint32_t startedAt = millis();
#if !defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS)
  HalDisplay::DisplayUpdateResult productionUpdateResult;
  const bool productionUsedChecked = productionWindowEnabled && renderer.supportsExperimentalWindowUpdates();
  GfxRenderer::LogicalRegion productionRequestedRegion;
  const bool productionStatusStable = productionStatusValid && currentDecision.state == productionPresentedState &&
                                      currentDecision.paceWpm == productionPresentedPaceWpm;
  const bool productionCanWindow =
      productionUsedChecked && !productionDisplayFailure.load(std::memory_order_acquire) &&
      !productionDisplayRecoveryRequested.load(std::memory_order_acquire) &&
      currentDecision.state == rsvp::State::Playing && currentDecision.pauseReason == rsvp::PauseReason::None &&
      message == nullptr && !panelVisible && !cleanup && drawnRegionValid && presentedRegionValid &&
      productionStatusStable && renderer.windowBaselineState() == HalDisplay::WindowBaselineState::Valid;
  if (productionCanWindow) {
    productionRequestedRegion = unionRegions(presentedRegion, drawnRegion);
    productionUpdateResult = renderer.displayWindowChecked(productionRequestedRegion);
  } else if (productionUsedChecked) {
    // The first frame, all UI/state transitions, and cleanup frames establish
    // a checked full baseline before a later line update is allowed.
    productionUpdateResult = renderer.displayBufferChecked(mode);
  } else {
    // Drivers without the checked-window capability retain the existing
    // blocking full-frame behavior.
    renderer.displayBuffer(mode);
  }
#else
  renderer.displayBuffer(mode);
#endif
#endif
  const uint32_t presentedAt = millis();
  const uint32_t duration =
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
      updateResult.durationMs;
#elif !defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS)
      productionUsedChecked ? productionUpdateResult.durationMs : presentedAt - startedAt;
#else
      presentedAt - startedAt;
#endif
  refreshStats.record(kind, duration);

#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
#if !defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS)
  if (productionUsedChecked) {
    if (!productionUpdateResult.succeeded()) {
      // Do not acknowledge a frame after a checked BUSY/error result. The
      // loop keeps controls queued while it waits for readiness, then asks
      // renderBook() for a checked full resync of this same frame.
      renderer.invalidateWindowBaseline();
      presentedRegionValid = false;
      productionStatusValid = false;
      productionDisplayRecoveryRequested.store(false, std::memory_order_release);
      productionDisplayFailure.store(true, std::memory_order_release);
      currentDecision.render = false;
      LOG_ERR("RSVP", "Checked display failed error=%u fallback=%u",
              static_cast<unsigned>(productionUpdateResult.error),
              static_cast<unsigned>(productionUpdateResult.fallback));
      return;
    }
#if defined(SIMULATOR)
    const auto trace = renderer.lastDisplayUpdateTrace();
    if (trace.valid && productionUpdateResult.actualKind == HalDisplay::DisplayUpdateKind::Window) {
      LOG_INF("RSVP",
              "production_roi stage=normal_window frame=%lu logical_x=%ld logical_y=%ld logical_w=%ld "
              "logical_h=%ld physical_x=%u physical_y=%u physical_w=%u physical_h=%u baseline_before=%u "
              "baseline_after=%u payload_bytes=%lu refresh_triggered=%u t=%lu",
              static_cast<unsigned long>(currentDecision.frame.id), static_cast<long>(productionRequestedRegion.x),
              static_cast<long>(productionRequestedRegion.y), static_cast<long>(productionRequestedRegion.width),
              static_cast<long>(productionRequestedRegion.height), static_cast<unsigned>(trace.x),
              static_cast<unsigned>(trace.y), static_cast<unsigned>(trace.width), static_cast<unsigned>(trace.height),
              static_cast<unsigned>(trace.baselineBefore), static_cast<unsigned>(trace.baselineAfter),
              static_cast<unsigned long>(trace.payloadBytes), static_cast<unsigned>(trace.refreshTriggered),
              static_cast<unsigned long>(presentedAt));
    } else if (trace.valid && (productionUpdateResult.actualKind == HalDisplay::DisplayUpdateKind::Full ||
                               productionUpdateResult.actualKind == HalDisplay::DisplayUpdateKind::Cleanup)) {
      LOG_INF("RSVP",
              "production_display stage=%s frame=%lu actual=%u baseline_before=%u baseline_after=%u "
              "physical_x=%u physical_y=%u physical_w=%u physical_h=%u payload_bytes=%lu refresh_triggered=%u t=%lu",
              productionUpdateResult.actualKind == HalDisplay::DisplayUpdateKind::Cleanup ? "normal_cleanup"
                                                                                          : "normal_full",
              static_cast<unsigned long>(currentDecision.frame.id),
              static_cast<unsigned>(productionUpdateResult.actualKind), static_cast<unsigned>(trace.baselineBefore),
              static_cast<unsigned>(trace.baselineAfter), static_cast<unsigned>(trace.x),
              static_cast<unsigned>(trace.y), static_cast<unsigned>(trace.width), static_cast<unsigned>(trace.height),
              static_cast<unsigned long>(trace.payloadBytes), static_cast<unsigned>(trace.refreshTriggered),
              static_cast<unsigned long>(presentedAt));
    }
#endif
    if (productionDisplayRecoveryRequested.load(std::memory_order_acquire)) {
      // The recovery latch is released only after the checked full call above
      // has completed successfully.
      productionDisplayRecoveryRequested.store(false, std::memory_order_release);
      productionDisplayFailure.store(false, std::memory_order_release);
    }
    if (message == nullptr && currentDecision.state == rsvp::State::Playing && drawnRegionValid && !cleanup) {
      // Carry only the last frame's successful content. The next request
      // unions that ROI with the newly drawn line so stale tails are covered
      // without growing the region across the whole reading session.
      presentedRegion = drawnRegion;
      presentedRegionValid = true;
      productionStatusValid = true;
      productionPresentedState = currentDecision.state;
      productionPresentedPaceWpm = currentDecision.paceWpm;
    } else {
      presentedRegionValid = false;
      productionStatusValid = false;
    }
  }
#endif
#endif

#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  const bool updateSucceeded = updateResult.succeeded();
  const auto diagnosticPhase = windowDiagnosticPhase;
  const auto trace = diagnosticTrace(renderer.lastDisplayUpdateTrace());
  windowDiagnosticState.recordAttempt(diagnosticPhase, operation, requestedRefresh(updateResult.requestedKind),
                                      actualRefresh(updateResult.actualKind), diagnosticError(updateResult.error),
                                      static_cast<uint8_t>(updateResult.error), duration, updateSucceeded, trace);
  if (!updateSucceeded) {
    windowBenchmark.record(presentedAt, presentedAt - frameStartedAt, duration, actualRefresh(updateResult.actualKind),
                           windowFallback(updateResult), false, observedCpuMhz());
    renderer.invalidateWindowBaseline();
    presentedRegionValid = false;
    windowProbePresentationPending = false;
    windowProbeAwaitingConfirm = false;
    windowProbeConfirmArmed = false;
    windowProbeFreshConfirmRequired = false;
    windowProbeWindowUpdateRequested = false;
    windowFailureAwaitingUser = true;
    windowFailureConfirmArmed = false;
    windowProbeAdvanceRequested = false;
    windowFailureRecoveryRequested = false;
    checkedDisplayFailure.store(true, std::memory_order_release);
    windowDiagnosticRunActive.store(false, std::memory_order_release);
    windowBenchmark.fail(presentedAt);
    windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
    windowReportPending = true;
    if (message == nullptr && session) {
      currentDecision = session->step({.nowMs = presentedAt,
                                       .action = rsvp::Action::FramePresentationFailed,
                                       .presentedFrameId = currentDecision.frame.id,
                                       .refreshDurationMs = duration});
    }
    currentDecision.render = false;
    return;
  }
  const auto successfulFallback =
      tightCandidateInvalid ? rsvp::WindowFallback::InvalidGeometry : windowFallback(updateResult);
  if (tightCandidateInvalid) {
    // An empty/invalid ink scan is never a successful tight candidate. Keep
    // the last valid region untouched and use the checked full update above as
    // the safe fallback; the tight probe itself must not be auto-confirmed.
    windowDiagnosticState.recordAttempt(windowDiagnosticPhase,
                                        windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe
                                            ? rsvp::WindowDiagnosticOperation::TightProbe
                                            : rsvp::WindowDiagnosticOperation::TightWindow,
                                        rsvp::WindowActualRefresh::Window, actualRefresh(updateResult.actualKind),
                                        rsvp::WindowDiagnosticError::InvalidRegion, 0, duration, false, trace);
  }
  const bool recoverySucceeded = operation == rsvp::WindowDiagnosticOperation::FullRecovery && updateSucceeded;
  if (recoverySucceeded) {
    // Keep navigation, modal UI, and auto-sleep blocked until the checked
    // recovery presentation itself succeeds, not merely until BUSY reads ready.
    windowFailureAwaitingUser = false;
    windowFailureRecoveryRequested = false;
    checkedDisplayFailure.store(false, std::memory_order_release);
    windowReportPending = true;
  }
  if (message == nullptr && drawnRegionValid) {
    if (tightStage) {
      if (tightRegionValid && (updateResult.actualKind == HalDisplay::DisplayUpdateKind::Window ||
                               updateResult.actualKind == HalDisplay::DisplayUpdateKind::Full)) {
        presentedRegion = tightRegion;
        presentedRegionValid = true;
      }
    } else {
      presentedRegion = drawnRegion;
      presentedRegionValid = true;
    }
  } else {
    presentedRegionValid = false;
  }
#endif

#if !defined(CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS) && \
    (!defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC)
  const auto& distribution = refreshStats.distribution(kind);
  LOG_INF("RSVP", "refresh=%s duration=%lums n=%lu min=%lu avg=%lu max=%lu buckets=%lu,%lu,%lu,%lu,%lu,%lu heap=%u",
          cleanup ? "cleanup" : "fast", static_cast<unsigned long>(duration),
          static_cast<unsigned long>(distribution.count), static_cast<unsigned long>(distribution.minimumMs),
          static_cast<unsigned long>(distribution.averageMs()), static_cast<unsigned long>(distribution.maximumMs),
          static_cast<unsigned long>(distribution.buckets[0]), static_cast<unsigned long>(distribution.buckets[1]),
          static_cast<unsigned long>(distribution.buckets[2]), static_cast<unsigned long>(distribution.buckets[3]),
          static_cast<unsigned long>(distribution.buckets[4]), static_cast<unsigned long>(distribution.buckets[5]),
          ESP.getFreeHeap());
#endif

  if (message == nullptr && !wordDoesNotFitPending.load()) {
#ifdef SIMULATOR
    const auto* group = currentDecision.frame.presentationGroup;
    if (group) {
      LOG_INF("RSVP", "group count=%u active=%u words=%s|%s|%s", group->count, group->activeIndex,
              group->tokens[0].text, group->count > 1 ? group->tokens[1].text : "",
              group->count > 2 ? group->tokens[2].text : "");
    }
#endif
    const auto acknowledgement = session->step({.nowMs =
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
                                                    presentedAt,
#else
                                                    millis(),
#endif
                                                .action = rsvp::Action::FramePresented,
                                                .presentedFrameId = currentDecision.frame.id,
                                                .refreshDurationMs = duration});
    if (!acknowledgement.presentationAccepted) {
      LOG_ERR("RSVP", "Ignored presentation acknowledgement for frame %lu",
              static_cast<unsigned long>(currentDecision.frame.id));
    }
#ifdef CROSSRSVP_MANUAL_SPEED_DIAGNOSTICS
    if (acknowledgement.presentationAccepted && currentDecision.state == rsvp::State::Playing &&
        speedProbe.record(currentDecision.frame.id, currentDecision.paceWpm, presentedAt, duration, cleanup)) {
      const auto* displayedGroup = currentDecision.frame.presentationGroup;
      LOG_INF("RSVP-SPEED",
              "sample run=%lu frame=%lu pace=%u kind=%s refresh_ms=%lu frame_ms=%lu interval_ms=%lu words=%u heap=%u",
              static_cast<unsigned long>(speedProbe.run()), static_cast<unsigned long>(currentDecision.frame.id),
              currentDecision.paceWpm, cleanup ? "cleanup" : "fast", static_cast<unsigned long>(duration),
              static_cast<unsigned long>(presentedAt - frameStartedAt),
              static_cast<unsigned long>(speedProbe.intervalMs()), displayedGroup ? displayedGroup->count : 1u,
              ESP.getFreeHeap());
    }
#endif
#if !defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) || !CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
    if (acknowledgement.checkpointRequested) checkpointRequestedFromRender.store(true);
#endif
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
    if (acknowledgement.presentationAccepted) {
      windowBenchmark.record(presentedAt, presentedAt - frameStartedAt, duration,
                             actualRefresh(updateResult.actualKind), successfulFallback, true, observedCpuMhz(),
                             currentDecision.frame.id);
    }
    if (acknowledgement.presentationAccepted && updateResult.actualKind == HalDisplay::DisplayUpdateKind::Window &&
        trace.valid) {
      windowBenchmark.recordRoi(trace.width, trace.height);
#if defined(SIMULATOR)
      if (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::WindowMeasurement ||
          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightMeasurement ||
          windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe) {
        LOG_INF("RSVP", "diagnostic_roi stage=%s frame=%lu x=%d y=%d w=%d h=%d px=%u py=%u pw=%u ph=%u t=%lu",
                rsvp::windowDiagnosticPhaseName(windowDiagnosticPhase),
                static_cast<unsigned long>(currentDecision.frame.id), requestedWindowRegion.x, requestedWindowRegion.y,
                requestedWindowRegion.width, requestedWindowRegion.height, static_cast<unsigned>(trace.x),
                static_cast<unsigned>(trace.y), static_cast<unsigned>(trace.width), static_cast<unsigned>(trace.height),
                static_cast<unsigned long>(presentedAt));
      }
#endif
    }
    if (acknowledgement.presentationAccepted && operation == rsvp::WindowDiagnosticOperation::FullResync &&
        (windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::FullSizePtl ||
         windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::LineCandidate)) {
      queueWindowProbeFrame(presentedAt);
    } else if (acknowledgement.presentationAccepted && operation == rsvp::WindowDiagnosticOperation::FullResync &&
               windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe && !tightCandidateInvalid) {
      queueWindowProbeFrame(presentedAt);
    } else if (acknowledgement.presentationAccepted && tightCandidateInvalid &&
               windowDiagnosticPhase == rsvp::WindowDiagnosticPhase::TightProbe) {
      windowProbePresentationPending = false;
      windowProbeAwaitingConfirm = false;
      windowProbeConfirmArmed = false;
      windowProbeFreshConfirmRequired = false;
      windowProbeWindowUpdateRequested = false;
      windowDiagnosticRunActive.store(false, std::memory_order_release);
      windowBenchmark.fail(presentedAt);
      windowDiagnosticPhase = rsvp::WindowDiagnosticPhase::Error;
      windowReportPending = true;
      currentDecision.render = false;
    } else if (acknowledgement.presentationAccepted && (operation == rsvp::WindowDiagnosticOperation::FullSizePtl ||
                                                        operation == rsvp::WindowDiagnosticOperation::LineCandidate ||
                                                        operation == rsvp::WindowDiagnosticOperation::TightProbe)) {
      // Keep the confirmed probe image on the panel until the operator
      // accepts it. No render is requested here, so a failed word cannot be
      // hidden by an automatic full redraw.
      windowProbePresentationPending = false;
      windowProbeAwaitingConfirm = true;
      windowProbeConfirmArmed = false;
      windowProbeFreshConfirmRequired = true;
      currentDecision.render = false;
    }
    if (acknowledgement.presentationAccepted && windowAutoPlayPending) {
      const auto playing = session->step({.nowMs = presentedAt, .action = rsvp::Action::TogglePlayback});
      currentDecision.state = playing.state;
      currentDecision.error = playing.error;
      currentDecision.pauseReason = playing.pauseReason;
      currentDecision.nextDeadlineMs = playing.nextDeadlineMs;
      currentDecision.paceWpm = playing.paceWpm;
      currentDecision.render = false;
      windowAutoPlayPending = false;
    }
#endif
  }
  if (fatalFallbackPending.load(std::memory_order_acquire)) fatalFallbackReady.store(true);
}
