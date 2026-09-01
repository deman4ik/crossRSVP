#include "RsvpReaderActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <optional>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReaderLaunchMode.h"
#include "activities/ActivityManager.h"

namespace {
rsvp::RsvpRefreshStats refreshStats;
}

bool RsvpReaderActivity::loadBook() {
  auto loadedEpub = makeUniqueNoThrow<Epub>(bookPath, "/.crosspoint");
  if (!loadedEpub) {
    LOG_ERR("RSVP", "Failed to allocate EPUB object");
    return false;
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
    return false;
  }
  loadedEpub->setupCacheDir();
  epub = std::move(loadedEpub);

  contentProvider = makeUniqueNoThrow<rsvp::ArduinoEpubContentProvider>(*epub);
  if (!contentProvider) {
    LOG_ERR("RSVP", "Failed to allocate EPUB content provider");
    return false;
  }
  source = makeUniqueNoThrow<rsvp::EpubVisibleTextSource>(*contentProvider);
  if (!source) {
    LOG_ERR("RSVP", "Failed to allocate visible EPUB stream");
    return false;
  }
  session = makeUniqueNoThrow<rsvp::RsvpSession>(*source);
  if (!session) {
    LOG_ERR("RSVP", "Failed to allocate session");
    return false;
  }

  currentDecision = session->step({.nowMs = millis()});
  if (currentDecision.switchToPaged) {
    switchToPagedPending = true;
    return true;
  }
  if (!currentDecision.render || currentDecision.state != rsvp::State::Paused) {
    LOG_ERR("RSVP", "No readable first token (state=%d error=%d)", static_cast<int>(currentDecision.state),
            static_cast<int>(currentDecision.error));
    return false;
  }
  return true;
}

void RsvpReaderActivity::loop() {
  if (!session) {
    finish();
    return;
  }

  if (switchToPagedPending) {
    activityManager.goToReader(bookPath, false, ReaderLaunchMode::Paged);
    return;
  }

  rsvp::Action action = rsvp::Action::None;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back))
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

  if (action == rsvp::Action::None) return;
  const auto decision = session->step({.nowMs = millis(), .action = action});
  if (decision.switchToPaged) {
    switchToPagedPending = true;
    activityManager.goToReader(bookPath, false, ReaderLaunchMode::Paged);
  }
}

void RsvpReaderActivity::renderBook() {
  if (!currentDecision.render || !currentDecision.frame.text) return;

  renderer.clearScreen();
  renderer.drawCenteredText(SETTINGS.getReaderFontId(), renderer.getScreenHeight() / 2, currentDecision.frame.text,
                            true, EpdFontFamily::REGULAR);

  const bool cleanup = forcedRefreshPending;
  forcedRefreshPending = false;
  const auto kind = cleanup ? rsvp::RefreshKind::Cleanup : rsvp::RefreshKind::Fast;
  const auto mode = cleanup ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH;
  const uint32_t startedAt = millis();
  renderer.displayBuffer(mode);
  const uint32_t duration = millis() - startedAt;
  refreshStats.record(kind, duration);

  const auto& distribution = refreshStats.distribution(kind);
  LOG_INF("RSVP",
          "refresh=%s duration=%lums n=%lu min=%lu avg=%lu max=%lu buckets=%lu,%lu,%lu,%lu,%lu,%lu heap=%u",
          cleanup ? "cleanup" : "fast", static_cast<unsigned long>(duration),
          static_cast<unsigned long>(distribution.count), static_cast<unsigned long>(distribution.minimumMs),
          static_cast<unsigned long>(distribution.averageMs()), static_cast<unsigned long>(distribution.maximumMs),
          static_cast<unsigned long>(distribution.buckets[0]), static_cast<unsigned long>(distribution.buckets[1]),
          static_cast<unsigned long>(distribution.buckets[2]), static_cast<unsigned long>(distribution.buckets[3]),
          static_cast<unsigned long>(distribution.buckets[4]), static_cast<unsigned long>(distribution.buckets[5]),
          ESP.getFreeHeap());

  const auto acknowledgement = session->step({.nowMs = millis(),
                                              .action = rsvp::Action::FramePresented,
                                              .presentedFrameId = currentDecision.frame.id,
                                              .refreshDurationMs = duration});
  if (!acknowledgement.presentationAccepted) {
    LOG_ERR("RSVP", "Ignored presentation acknowledgement for frame %lu",
            static_cast<unsigned long>(currentDecision.frame.id));
  }
}
