#include <HalDisplay.h>
#include <HalGPIO.h>
#include <XteinkDetect.h>

// Global HalDisplay instance
HalDisplay display;

#define SD_SPI_MISO 7

HalDisplay::HalDisplay() : einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY) {}

HalDisplay::~HalDisplay() {}

HalDisplay::Controller HalDisplay::getController() const { return BoardConfig::ACTIVE.displayController; }

HalDisplay::ControllerDetection HalDisplay::controllerDetection() const {
  ControllerDetection detection{getController(), ControllerConfidence::Inconclusive, gpio.deviceIsX3()};
  if (!detection.isX3) return detection;

  const freeink::XteinkDisplayProbeDiag& probe = freeink::getXteinkDisplayProbeDiag();
  if (!probe.valid) return detection;

  const auto verdict = static_cast<freeink::DisplayControllerVerdict>(probe.verdict);
  if (detection.controller == Controller::UC8279 && verdict == freeink::DisplayControllerVerdict::Uc81xxConfirmed) {
    detection.confidence = ControllerConfidence::Confirmed;
  } else if (detection.controller == Controller::UC8253 &&
             verdict == freeink::DisplayControllerVerdict::PrimaryAssumed) {
    detection.confidence = ControllerConfidence::Assumed;
  }
  return detection;
}

void HalDisplay::begin(bool seamless) {
  // Set X3-specific panel mode before initializing.
  if (gpio.deviceIsX3()) {
    einkDisplay.setDisplayX3();
  }

  einkDisplay.begin();

  if (seamless) {
    // Defuse the SDK's X3 _x3InitialFullSyncsRemaining counter (no-op on X4)
    // so the first paint isn't promoted to FULL (~770ms). Skips the wakeup-
    // gated requestResync() below for the same reason.
    einkDisplay.skipInitialResync();
    return;
  }
  // Request resync after specific wakeup events to ensure clean display state.
  const auto wakeupReason = gpio.getWakeupReason();
  if (wakeupReason == HalGPIO::WakeupReason::PowerButton || wakeupReason == HalGPIO::WakeupReason::AfterFlash ||
      wakeupReason == HalGPIO::WakeupReason::Other) {
    einkDisplay.requestResync();
  }
}

void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  einkDisplay.drawImage(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool fromProgmem) const {
  einkDisplay.drawImageTransparent(imageData, x, y, w, h, fromProgmem);
}

EInkDisplay::RefreshMode convertRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

namespace {

HalDisplay::DisplayUpdateKind convertUpdateKind(const freeink::DisplayUpdateKind kind) {
  switch (kind) {
    case freeink::DisplayUpdateKind::Full:
      return HalDisplay::DisplayUpdateKind::Full;
    case freeink::DisplayUpdateKind::Window:
      return HalDisplay::DisplayUpdateKind::Window;
    case freeink::DisplayUpdateKind::None:
    default:
      return HalDisplay::DisplayUpdateKind::None;
  }
}

HalDisplay::DisplayUpdateFallback convertUpdateFallback(const freeink::DisplayUpdateFallback fallback) {
  switch (fallback) {
    case freeink::DisplayUpdateFallback::ExperimentalDisabled:
      return HalDisplay::DisplayUpdateFallback::ExperimentalDisabled;
    case freeink::DisplayUpdateFallback::UnsupportedDriver:
      return HalDisplay::DisplayUpdateFallback::UnsupportedDriver;
    case freeink::DisplayUpdateFallback::Inverted:
      return HalDisplay::DisplayUpdateFallback::Inverted;
    case freeink::DisplayUpdateFallback::BaselineInvalid:
      return HalDisplay::DisplayUpdateFallback::BaselineInvalid;
    case freeink::DisplayUpdateFallback::None:
    default:
      return HalDisplay::DisplayUpdateFallback::None;
  }
}

HalDisplay::DisplayUpdateResult convertUpdateResult(const freeink::DisplayUpdateResult& source) {
  HalDisplay::DisplayUpdateResult result;
  result.requestedKind = convertUpdateKind(source.requestedKind);
  result.actualKind = convertUpdateKind(source.actualKind);
  result.error = source.error;
  result.fallback = convertUpdateFallback(source.fallback);
  result.durationMs = source.durationMs;
  return result;
}

}  // namespace

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayBuffer(convertRefreshMode(mode), turnOffScreen);
}

HalDisplay::DisplayUpdateResult HalDisplay::displayBufferChecked(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }
  DisplayUpdateResult result =
      convertUpdateResult(einkDisplay.displayBufferChecked(convertRefreshMode(mode), turnOffScreen));
  if (mode != RefreshMode::FAST_REFRESH) {
    result.requestedKind = DisplayUpdateKind::Cleanup;
    if (result.actualKind == DisplayUpdateKind::Full) result.actualKind = DisplayUpdateKind::Cleanup;
  }
  return result;
}

HalDisplay::DisplayUpdateResult HalDisplay::displayWindowChecked(uint16_t x, uint16_t y, uint16_t width,
                                                                 uint16_t height, bool turnOffScreen) {
  DisplayUpdateResult result =
      convertUpdateResult(einkDisplay.displayWindowChecked(x, y, width, height, turnOffScreen));
  if (result.fallback == DisplayUpdateFallback::ExperimentalDisabled &&
      windowGateFallback != DisplayUpdateFallback::None) {
    result.fallback = windowGateFallback;
  }
  return result;
}

void HalDisplay::setExperimentalWindowUpdates(bool enabled) {
  bool eligible = false;
  windowGateFallback = DisplayUpdateFallback::ExperimentalDisabled;
#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC
  const ControllerDetection detection = controllerDetection();
  if (enabled && !detection.isX3) {
    windowGateFallback = DisplayUpdateFallback::UnsupportedModel;
  } else if (enabled && detection.controller != Controller::UC8253) {
    windowGateFallback = DisplayUpdateFallback::UnsupportedController;
  } else if (enabled && detection.confidence == ControllerConfidence::Inconclusive) {
    windowGateFallback = DisplayUpdateFallback::InconclusiveController;
  } else if (enabled && !einkDisplay.supportsExperimentalWindowUpdates()) {
    windowGateFallback = DisplayUpdateFallback::UnsupportedDriver;
  } else if (enabled) {
    eligible = true;
    windowGateFallback = DisplayUpdateFallback::None;
  }
#else
  const ControllerDetection detection = controllerDetection();
  if (enabled && detection.isX3 && detection.confidence == ControllerConfidence::Inconclusive) {
    // X3 controller auto-detection is the safety boundary for the C3 panel;
    // X4/X4 Pro do not expose that probe and are gated by their selected
    // driver's capability instead.
    windowGateFallback = DisplayUpdateFallback::InconclusiveController;
  } else if (enabled && !einkDisplay.supportsExperimentalWindowUpdates()) {
    windowGateFallback = DisplayUpdateFallback::UnsupportedDriver;
  } else if (enabled) {
    eligible = true;
    windowGateFallback = DisplayUpdateFallback::None;
  }
#endif
  einkDisplay.setExperimentalWindowUpdates(eligible);
}

bool HalDisplay::supportsExperimentalWindowUpdates() const {
  return windowGateFallback == DisplayUpdateFallback::None && einkDisplay.supportsExperimentalWindowUpdates();
}

void HalDisplay::invalidateWindowBaseline() { einkDisplay.invalidateWindowBaseline(); }

HalDisplay::WindowBaselineState HalDisplay::windowBaselineState() const { return einkDisplay.windowBaselineState(); }

HalDisplay::DisplayUpdateTrace HalDisplay::lastDisplayUpdateTrace() const {
  return einkDisplay.lastDisplayUpdateTrace();
}

bool HalDisplay::checkedDisplayReady() const { return einkDisplay.checkedDisplayReady(); }

void HalDisplay::displayBufferAsync(HalDisplay::RefreshMode mode) {
  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayBufferAsyncNoShadow(convertRefreshMode(mode));
}

void HalDisplay::waitRefreshComplete() { einkDisplay.waitRefreshComplete(); }

bool HalDisplay::supportsAsyncRefresh() const { return einkDisplay.supportsAsyncRefresh(); }

HalDisplay::GrayscaleCapabilities HalDisplay::grayscaleCapabilities(GrayscaleMode mode) const {
  return einkDisplay.grayscaleCapabilities(mode);
}

bool HalDisplay::supportsAsyncGrayscaleBase() const { return grayscaleCapabilities().asyncBase; }

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.refreshDisplay(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::setInverted(bool inverted) { einkDisplay.setInverted(inverted); }

bool HalDisplay::toggleInverted() { return einkDisplay.toggleInverted(); }

bool HalDisplay::isInverted() const { return einkDisplay.isInverted(); }

void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* sizeOut) { return einkDisplay.lendBuildStorage(sizeOut); }

void HalDisplay::returnFrameBufferStorage() { einkDisplay.returnBuildStorage(); }

bool HalDisplay::displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback, bool turnOffScreen) {
  if (gpio.deviceIsX3() && fallback == HALF_REFRESH) einkDisplay.requestResync();
  return einkDisplay.displayGrayscaleBase(mode, static_cast<EInkDisplay::RefreshMode>(fallback), turnOffScreen);
}

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  einkDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) {
  // X3: a HALF fallback means the caller wants a clean base (e.g. the sleep
  // cover, a full-screen swap from arbitrary prior content). Without this, the
  // X3 grayscale base takes its gentle differential happy path and the prior
  // home/reader frame ghosts through the soft aa_pre_bw_mid waveform. Forcing a
  // resync makes displayGrayscaleBase clear first, matching displayBuffer(HALF).
  // The reader's FAST path is deliberately left on the differential path so
  // per-page grayscale stays cheap.
  if (gpio.deviceIsX3() && fallback == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayGrayscaleBase(convertRefreshMode(fallback), turnOffScreen);
}

void HalDisplay::preconditionGrayscale() { einkDisplay.preconditionGrayscale(); }

void HalDisplay::preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  einkDisplay.preconditionGrayscale(x, y, w, h);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { einkDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { einkDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) { einkDisplay.cleanupGrayscaleBuffers(bwBuffer); }

void HalDisplay::displayGrayBuffer(bool turnOffScreen) { einkDisplay.displayGrayBuffer(turnOffScreen); }

void HalDisplay::writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows) {
  einkDisplay.writeGrayscalePlaneStrip(lsbPlane ? EInkDisplay::GRAY_PLANE_LSB : EInkDisplay::GRAY_PLANE_MSB, rows,
                                       yStart, numRows);
}

bool HalDisplay::supportsStripGrayscale() const { return grayscaleCapabilities().stripUploads; }

bool HalDisplay::combinesGrayscaleBase() const { return grayscaleCapabilities().base == GrayscaleBase::Combined; }

uint16_t HalDisplay::getDisplayWidth() const { return einkDisplay.getDisplayWidth(); }

uint16_t HalDisplay::getDisplayHeight() const { return einkDisplay.getDisplayHeight(); }

uint16_t HalDisplay::getDisplayWidthBytes() const { return einkDisplay.getDisplayWidthBytes(); }

uint32_t HalDisplay::getBufferSize() const { return einkDisplay.getBufferSize(); }
