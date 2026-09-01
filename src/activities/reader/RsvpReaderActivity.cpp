#include "RsvpReaderActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#include <optional>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReaderLaunchMode.h"
#include "ReaderUtils.h"
#include "activities/ActivityManager.h"
#include "fontIds.h"

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
  rsvp::RsvpPacingConfig pacing;
  pacing.paceWpm = SETTINGS.rsvpPaceWpm;
  pacing.clausePausePercent = static_cast<uint16_t>(SETTINGS.rsvpClausePauseTenths) * 10;
  pacing.sentencePausePercent = static_cast<uint16_t>(SETTINGS.rsvpSentencePauseTenths) * 10;
  pacing.paragraphPausePercent = static_cast<uint16_t>(SETTINGS.rsvpParagraphPauseTenths) * 10;
  session = makeUniqueNoThrow<rsvp::RsvpSession>(*source, rsvp::ResumeAnchor{}, pacing);
  if (!session) {
    LOG_ERR("RSVP", "Failed to allocate session");
    return false;
  }

  currentDecision = session->step({.nowMs = millis()});
  if (!currentDecision.render && currentDecision.pauseReason == rsvp::PauseReason::None &&
      currentDecision.state != rsvp::State::Finished) {
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

  const auto decision = session->step({.nowMs = millis(), .action = action});
  if (decision.switchToPaged) {
    switchToPagedPending = true;
    activityManager.goToReader(bookPath, false, ReaderLaunchMode::Paged);
    return;
  }
  applyDecision(decision);
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
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, message, true, EpdFontFamily::BOLD);
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
  }
}
