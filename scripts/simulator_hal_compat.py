#!/usr/bin/env python3
"""Add the typed HAL surface expected by CrossPoint to the pinned simulator.

The simulator revision is intentionally kept pinned for reproducible evidence,
but its native HalDisplay shim predates the SDK grayscale capability API. This
pre-script patches only the dependency copy in .pio/libdeps at build time.
"""

from pathlib import Path

Import("env")

libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
simulator_dir = libdeps_dir / env.get("PIOENV") / "simulator"
header = simulator_dir / "src" / "HalDisplay.h"
source = simulator_dir / "src" / "HalDisplay.cpp"
gpio_header = simulator_dir / "src" / "HalGPIO.h"
gpio_source = simulator_dir / "src" / "HalGPIO.cpp"

if not all(path.is_file() for path in (header, source, gpio_header, gpio_source)):
    raise RuntimeError(f"simulator HAL compatibility patch: missing simulator HAL source under {simulator_dir}")

header_text = header.read_text(encoding="utf-8")
source_text = source.read_text(encoding="utf-8")
gpio_header_text = gpio_header.read_text(encoding="utf-8")
gpio_source_text = gpio_source.read_text(encoding="utf-8")
original_header = header_text
original_source = source_text
original_gpio_header = gpio_header_text
original_gpio_source = gpio_source_text

legacy_header = "#include <EInkDisplay.h>\n\nclass HalDisplay {"
if "using GrayscaleMode = freeink::GrayscaleMode;" not in header_text:
    if legacy_header not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: unexpected header context: {header}")
    header_text = header_text.replace(
        legacy_header,
        "#include <EInkDisplay.h>\n#include <BoardConfig.h>\n#include <DisplayUpdateResult.h>\n"
        "#include <GrayscaleCapabilities.h>\n\nclass HalDisplay {",
        1,
    )
    anchor = "class HalDisplay {\npublic:\n"
    if anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing class anchor: {header}")
    additions = (
        "class HalDisplay {\npublic:\n"
        "  using Controller = BoardConfig::DisplayController;\n"
        "  Controller getController() const;\n"
        "  using GrayscaleMode = freeink::GrayscaleMode;\n"
        "  using GrayscaleCapabilities = freeink::GrayscaleCapabilities;\n"
        "  using GrayscaleBase = freeink::GrayscaleBase;\n"
        "  using GrayscaleEncoding = freeink::GrayscaleEncoding;\n"
        "  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode mode = GrayscaleMode::Overlay) const;\n"
    )
    header_text = header_text.replace(anchor, additions, 1)

if "#include <DisplayUpdateResult.h>" not in header_text:
    include_anchor = "#include <BoardConfig.h>\n"
    if include_anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing BoardConfig include: {header}")
    header_text = header_text.replace(include_anchor, include_anchor + "#include <DisplayUpdateResult.h>\n", 1)

if "enum class ControllerConfidence" not in header_text:
    controller_anchor = "  Controller getController() const;\n"
    if controller_anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing controller anchor: {header}")
    controller_contract = (
        controller_anchor
        + "  enum class ControllerConfidence : uint8_t { Confirmed, Assumed, Inconclusive };\n"
        + "  struct ControllerDetection {\n"
        + "    Controller controller;\n"
        + "    ControllerConfidence confidence;\n"
        + "    bool isX3;\n"
        + "  };\n"
        + "  ControllerDetection controllerDetection() const;\n"
        + "  enum class DisplayUpdateKind : uint8_t { None, Full, Window, Cleanup };\n"
        + "  using DisplayUpdateError = freeink::DisplayUpdateError;\n"
        + "  enum class DisplayUpdateFallback : uint8_t {\n"
        + "    None, ExperimentalDisabled, UnsupportedModel, UnsupportedController,\n"
        + "    InconclusiveController, UnsupportedDriver, Inverted, BaselineInvalid, RefreshPromoted\n"
        + "  };\n"
        + "  struct DisplayUpdateResult {\n"
        + "    DisplayUpdateKind requestedKind = DisplayUpdateKind::None;\n"
        + "    DisplayUpdateKind actualKind = DisplayUpdateKind::None;\n"
        + "    DisplayUpdateError error = DisplayUpdateError::None;\n"
        + "    DisplayUpdateFallback fallback = DisplayUpdateFallback::None;\n"
        + "    uint32_t durationMs = 0;\n"
        + "    constexpr bool succeeded() const {\n"
        + "      return error == DisplayUpdateError::None && actualKind != DisplayUpdateKind::None;\n"
        + "    }\n"
        + "  };\n"
        + "  using WindowBaselineState = freeink::WindowBaselineState;\n"
    )
    header_text = header_text.replace(controller_anchor, controller_contract, 1)

if "DisplayUpdateResult displayBufferChecked" not in header_text:
    checked_anchor = "  void displayBufferAsync(RefreshMode mode = RefreshMode::FAST_REFRESH);\n"
    if checked_anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing displayBufferAsync anchor: {header}")
    checked_contract = (
        checked_anchor
        + "  DisplayUpdateResult displayBufferChecked(RefreshMode mode = RefreshMode::FAST_REFRESH,\n"
        + "                                           bool turnOffScreen = false);\n"
        + "  DisplayUpdateResult displayWindowChecked(uint16_t x, uint16_t y, uint16_t width, uint16_t height,\n"
        + "                                           bool turnOffScreen = false);\n"
        + "  void setExperimentalWindowUpdates(bool enabled);\n"
        + "  bool supportsExperimentalWindowUpdates() const;\n"
        + "  void invalidateWindowBaseline();\n"
        + "  WindowBaselineState windowBaselineState() const;\n"
        + "  bool checkedDisplayReady() const;\n"
    )
    header_text = header_text.replace(checked_anchor, checked_contract, 1)

if "DisplayUpdateFallback windowGateFallback" not in header_text:
    private_anchor = "  bool inverted = false;\n"
    if private_anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing private display anchor: {header}")
    header_text = header_text.replace(
        private_anchor,
        private_anchor
        + "  DisplayUpdateFallback windowGateFallback = DisplayUpdateFallback::ExperimentalDisabled;\n"
        + "  WindowBaselineState baselineState = WindowBaselineState::Invalid;\n"
        + "  bool displayReady = true;\n",
        1,
    )

legacy_method = "  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH,\n                            bool turnOffScreen = false);"
if "bool displayGrayscaleBase(GrayscaleMode mode" not in header_text:
    if legacy_method not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing grayscale method: {header}")
    header_text = header_text.replace(
        legacy_method,
        legacy_method
        + "\n  bool displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback = HALF_REFRESH,\n                            bool turnOffScreen = false);",
        1,
    )

async_anchor = "  bool supportsAsyncRefresh() const;"
if "supportsAsyncGrayscaleBase" not in header_text:
    if async_anchor not in header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing async anchor: {header}")
    header_text = header_text.replace(async_anchor, async_anchor + "\n  bool supportsAsyncGrayscaleBase() const;", 1)

if "HalDisplay::Controller HalDisplay::getController() const" not in source_text:
    source_anchor = "HalDisplay::~HalDisplay() {}\n"
    if source_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing source anchor: {source}")
    source_text = source_text.replace(
        source_anchor,
        source_anchor
        + "HalDisplay::Controller HalDisplay::getController() const { return BoardConfig::ACTIVE.displayController; }\n"
        + "HalDisplay::GrayscaleCapabilities HalDisplay::grayscaleCapabilities(GrayscaleMode mode) const {\n"
        + "  if (mode != GrayscaleMode::Overlay) return {};\n"
        + "  return {freeink::GrayscaleEncoding::OverlayMasks,\n"
        + "          combinesGrayscaleBase() ? freeink::GrayscaleBase::Combined : freeink::GrayscaleBase::Separate,\n"
        + "          supportsStripGrayscale(), supportsAsyncRefresh(), false};\n"
        + "}\n"
        + "bool HalDisplay::supportsAsyncGrayscaleBase() const { return supportsAsyncRefresh(); }\n",
        1,
    )

if "void renderBwRegion(const uint8_t *fb" not in source_text:
    region_anchor = "void clearGrayscalePlanes() {\n"
    if region_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing BW render anchor: {source}")
    source_text = source_text.replace(
        region_anchor,
        "void renderBwRegion(const uint8_t *fb, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {\n"
        "  const std::lock_guard<std::mutex> lock(pixelBufMutex);\n"
        "  const bool invert = display.isInverted();\n"
        "  for (uint16_t row = y; row < static_cast<uint16_t>(y + height); ++row) {\n"
        "    for (uint16_t column = x; column < static_cast<uint16_t>(x + width); ++column) {\n"
        "      const bool white = getBit(fb, column, row);\n"
        "      pixelBuf[row * HalDisplay::DISPLAY_WIDTH + column] =\n"
        "          (white != invert) ? 0xFFFFFFFFu : 0xFF000000u;\n"
        "    }\n"
        "  }\n"
        "  pendingPresent.store(true);\n"
        "}\n\n"
        "bool consumeWindowFailure(const char *expected) {\n"
        "  static bool consumed = false;\n"
        "  if (consumed) return false;\n"
        "  const char *configured = std::getenv(\"CROSSPOINT_SIM_WINDOW_FAILURE\");\n"
        "  if (!configured || std::strcmp(configured, expected) != 0) return false;\n"
        "  consumed = true;\n"
        "  return true;\n"
        "}\n\n"
        "bool simulatorWindowRecoveryEnabled() {\n"
        "  const char *configured = std::getenv(\"CROSSPOINT_SIM_WINDOW_RECOVER\");\n"
        "  return configured && std::strcmp(configured, \"1\") == 0;\n"
        "}\n\n"
        + region_anchor,
        1,
    )

if "HalDisplay::ControllerDetection HalDisplay::controllerDetection() const" not in source_text:
    detection_anchor = "HalDisplay::Controller HalDisplay::getController() const { return BoardConfig::ACTIVE.displayController; }\n"
    if detection_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing controller source anchor: {source}")
    source_text = source_text.replace(
        detection_anchor,
        detection_anchor
        + "HalDisplay::ControllerDetection HalDisplay::controllerDetection() const {\n"
        + "#if defined(SIMULATOR_DEVICE_X3)\n"
        + "#if defined(SIMULATOR_DISPLAY_UC8279)\n"
        + "  return {getController(), ControllerConfidence::Confirmed, true};\n"
        + "#else\n"
        + "  return {getController(), ControllerConfidence::Assumed, true};\n"
        + "#endif\n"
        + "#else\n"
        + "  return {getController(), ControllerConfidence::Inconclusive, false};\n"
        + "#endif\n"
        + "}\n",
        1,
    )

if "HalDisplay::DisplayUpdateResult HalDisplay::displayBufferChecked" not in source_text:
    checked_source_anchor = "void HalDisplay::waitRefreshComplete() {}\n"
    if checked_source_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing checked source anchor: {source}")
    source_text = source_text.replace(
        checked_source_anchor,
        "HalDisplay::DisplayUpdateResult HalDisplay::displayBufferChecked(RefreshMode mode, bool turnOffScreen) {\n"
        "  DisplayUpdateResult result;\n"
        "  result.requestedKind = mode == FAST_REFRESH ? DisplayUpdateKind::Full : DisplayUpdateKind::Cleanup;\n"
        "  if (!checkedDisplayReady()) {\n"
        "    result.error = DisplayUpdateError::BusyNotReady;\n"
        "    return result;\n"
        "  }\n"
        "  const unsigned long started = millis();\n"
        "  displayReady = true;\n"
        "  baselineState = WindowBaselineState::Resyncing;\n"
        "  refreshDisplay(mode, turnOffScreen);\n"
        "  baselineState = turnOffScreen ? WindowBaselineState::Invalid : WindowBaselineState::Valid;\n"
        "  result.actualKind = result.requestedKind;\n"
        "  result.durationMs = millis() - started;\n"
        "  return result;\n"
        "}\n\n"
        "HalDisplay::DisplayUpdateResult HalDisplay::displayWindowChecked(\n"
        "    uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool turnOffScreen) {\n"
        "  DisplayUpdateResult result;\n"
        "  result.requestedKind = DisplayUpdateKind::Window;\n"
        "  if (width == 0 || height == 0 || (x & 0x7u) != 0 || (width & 0x7u) != 0 ||\n"
        "      static_cast<uint32_t>(x) + width > DISPLAY_WIDTH ||\n"
        "      static_cast<uint32_t>(y) + height > DISPLAY_HEIGHT) {\n"
        "    result.error = DisplayUpdateError::InvalidRegion;\n"
        "    return result;\n"
        "  }\n"
        "  if (!checkedDisplayReady() || consumeWindowFailure(\"busy_not_ready_once\")) {\n"
        "    result.error = DisplayUpdateError::BusyNotReady;\n"
        "    return result;\n"
        "  }\n"
        "  DisplayUpdateFallback fallback = windowGateFallback;\n"
        "  if (fallback == DisplayUpdateFallback::None && inverted) fallback = DisplayUpdateFallback::Inverted;\n"
        "  if (fallback == DisplayUpdateFallback::None && baselineState != WindowBaselineState::Valid)\n"
        "    fallback = DisplayUpdateFallback::BaselineInvalid;\n"
        "  if (fallback != DisplayUpdateFallback::None) {\n"
        "    result = displayBufferChecked(FAST_REFRESH, turnOffScreen);\n"
        "    result.requestedKind = DisplayUpdateKind::Window;\n"
        "    result.fallback = fallback;\n"
        "    return result;\n"
        "  }\n"
        "  if (consumeWindowFailure(\"busy_timeout_once\")) {\n"
        "    displayReady = false;\n"
        "    baselineState = WindowBaselineState::Invalid;\n"
        "    result.error = DisplayUpdateError::BusyTimeout;\n"
        "    return result;\n"
        "  }\n"
        "  const unsigned long started = millis();\n"
        "  renderBwRegion(getFrameBuffer(), x, y, width, height);\n"
        "  baselineState = turnOffScreen ? WindowBaselineState::Invalid : WindowBaselineState::Valid;\n"
        "  result.actualKind = DisplayUpdateKind::Window;\n"
        "  result.durationMs = millis() - started;\n"
        "  return result;\n"
        "}\n\n"
        "void HalDisplay::setExperimentalWindowUpdates(bool enabled) {\n"
        "  windowGateFallback = DisplayUpdateFallback::ExperimentalDisabled;\n"
        "#if defined(CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC) && CROSSPOINT_RSVP_WINDOW_DIAGNOSTIC\n"
        "  const ControllerDetection detection = controllerDetection();\n"
        "  if (enabled && !detection.isX3) {\n"
        "    windowGateFallback = DisplayUpdateFallback::UnsupportedModel;\n"
        "  } else if (enabled && detection.controller != Controller::UC8253) {\n"
        "    windowGateFallback = DisplayUpdateFallback::UnsupportedController;\n"
        "  } else if (enabled && detection.confidence == ControllerConfidence::Inconclusive) {\n"
        "    windowGateFallback = DisplayUpdateFallback::InconclusiveController;\n"
        "  } else if (enabled) {\n"
        "    windowGateFallback = DisplayUpdateFallback::None;\n"
        "  }\n"
        "#else\n"
        "  (void)enabled;\n"
        "#endif\n"
        "}\n\n"
        "bool HalDisplay::supportsExperimentalWindowUpdates() const {\n"
        "  return windowGateFallback == DisplayUpdateFallback::None;\n"
        "}\n\n"
        "void HalDisplay::invalidateWindowBaseline() { baselineState = WindowBaselineState::Invalid; }\n\n"
        "HalDisplay::WindowBaselineState HalDisplay::windowBaselineState() const { return baselineState; }\n\n"
        "bool HalDisplay::checkedDisplayReady() const {\n"
        "  return displayReady || simulatorWindowRecoveryEnabled();\n"
        "}\n\n"
        + checked_source_anchor,
        1,
    )

# Migrate dependency copies patched by an earlier version of this hook. The
# simulator package is cached between PlatformIO runs, so insertion-only logic
# would otherwise leave the old turn-off baseline semantics in place.
checked_full_baseline = (
    "  refreshDisplay(mode, turnOffScreen);\n"
    "  baselineState = WindowBaselineState::Valid;\n"
)
if checked_full_baseline in source_text:
    source_text = source_text.replace(
        checked_full_baseline,
        "  refreshDisplay(mode, turnOffScreen);\n"
        "  baselineState = turnOffScreen ? WindowBaselineState::Invalid : WindowBaselineState::Valid;\n",
        1,
    )

checked_window_baseline = (
    "  renderBwRegion(getFrameBuffer(), x, y, width, height);\n"
    "  result.actualKind = DisplayUpdateKind::Window;\n"
)
if checked_window_baseline in source_text:
    source_text = source_text.replace(
        checked_window_baseline,
        "  renderBwRegion(getFrameBuffer(), x, y, width, height);\n"
        "  baselineState = turnOffScreen ? WindowBaselineState::Invalid : WindowBaselineState::Valid;\n"
        "  result.actualKind = DisplayUpdateKind::Window;\n",
        1,
    )

if "// Checked-window baseline is invalid after any legacy presentation." not in source_text:
    refresh_anchor = "void HalDisplay::refreshDisplay(RefreshMode /*mode*/, bool /*turnOffScreen*/) {\n"
    if refresh_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing refreshDisplay anchor: {source}")
    source_text = source_text.replace(
        refresh_anchor,
        refresh_anchor + "  // Checked-window baseline is invalid after any legacy presentation.\n"
        "  baselineState = WindowBaselineState::Invalid;\n",
        1,
    )

if "bool HalDisplay::displayGrayscaleBase(GrayscaleMode mode" not in source_text:
    source_anchor = "void HalDisplay::preconditionGrayscale() {}\n"
    if source_anchor not in source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing typed grayscale anchor: {source}")
    source_text = source_text.replace(
        source_anchor,
        "bool HalDisplay::displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback, bool turnOffScreen) {\n"
        "  if (mode != GrayscaleMode::Overlay) return false;\n"
        "  displayGrayscaleBase(fallback, turnOffScreen);\n"
        "  return true;\n"
        "}\n"
        + source_anchor,
        1,
    )

if "bool rawInputActive()" not in gpio_header_text:
    gpio_anchor = "  bool wasAnyPressed() const;\n"
    if gpio_anchor not in gpio_header_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing raw input header anchor: {gpio_header}")
    gpio_header_text = gpio_header_text.replace(gpio_anchor, gpio_anchor + "  bool rawInputActive();\n", 1)

if "bool HalGPIO::rawInputActive()" not in gpio_source_text:
    gpio_anchor = "bool HalGPIO::isPressed(uint8_t buttonIndex) const {\n"
    if gpio_anchor not in gpio_source_text:
        raise RuntimeError(f"simulator HAL compatibility patch: missing raw input source anchor: {gpio_source}")
    gpio_source_text = gpio_source_text.replace(
        gpio_anchor,
        "bool HalGPIO::rawInputActive() {\n"
        "  for (uint8_t button = 0; button < NUM_BUTTONS; ++button) {\n"
        "    if (isPressed(button)) return true;\n"
        "  }\n"
        "  // Touch activity is edge-latched by update(); observe its down edge\n"
        "  // so a short contact cannot be hidden by the idle sleep slice.\n"
        "  return wasTouchActivity();\n"
        "}\n\n"
        + gpio_anchor,
        1,
    )

if header_text != original_header:
    header.write_text(header_text, encoding="utf-8")
if source_text != original_source:
    source.write_text(source_text, encoding="utf-8")
if gpio_header_text != original_gpio_header:
    gpio_header.write_text(gpio_header_text, encoding="utf-8")
if gpio_source_text != original_gpio_source:
    gpio_source.write_text(gpio_source_text, encoding="utf-8")
