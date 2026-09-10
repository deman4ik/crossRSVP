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
        "#include <EInkDisplay.h>\n#include <BoardConfig.h>\n#include <GrayscaleCapabilities.h>\n\nclass HalDisplay {",
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
