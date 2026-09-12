#!/usr/bin/env python3
"""Exercise the autonomous X3 RSVP full/window/tight diagnostic in the simulator."""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import importlib.util
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[0]
FIXTURE_BUILDER = (SCRIPT_DIR / "build_rsvp_speed_fixture.py" if
                   (SCRIPT_DIR / "build_rsvp_speed_fixture.py").is_file() else
                   REPO_ROOT / "scripts" / "build_rsvp_speed_fixture.py")
SIM_HELPER = (SCRIPT_DIR / "qualify_rsvp_simulator.py" if
              (SCRIPT_DIR / "qualify_rsvp_simulator.py").is_file() else
              REPO_ROOT / "scripts" / "qualify_rsvp_simulator.py")
ORIENTATIONS = range(4)
ORIENTATION_NAMES = ("portrait", "landscape_cw", "portrait_inverted", "landscape_ccw")
X3_PANEL_WIDTH = 792
X3_PANEL_HEIGHT = 528
REPORT = Path("fs_/.crosspoint/rsvp-window-test.csv")
START_RSVP = "1200:ENTER:900"
START_DIAGNOSTIC = "3600:ENTER:900"
# The diagnostic now pauses after each short probe. Keep these confirmations
# comfortably after the probe screenshot so the scripted input cannot race
# the panel update on a slow host.
FULL_SIZE_PTL_CONFIRM = "10000:ENTER"
LINE_CANDIDATE_CONFIRM = "16000:ENTER"
# The existing full/window branches each run for 60 seconds.  Leave a
# recovery margin after the window branch before capturing the tight probe;
# an earlier Confirm is consumed by the still-running window measurement.
TIGHT_PROBE_SCREENSHOT_MS = 170000
TIGHT_PROBE_CONFIRM = "175000:ENTER"
# A single fixed pair can alias the deterministic fixture at one orientation.
# Capture unevenly spaced frames and select the first pair that visibly changes.
TIGHT_MEASUREMENT_SCREENSHOTS_MS = (220000, 220137, 220419, 220883, 221337)
TIGHT_MEASUREMENT_A_MS = TIGHT_MEASUREMENT_SCREENSHOTS_MS[0]
TIGHT_MEASUREMENT_B_MS = TIGHT_MEASUREMENT_SCREENSHOTS_MS[-1]
TIGHT_RESULTS_MS = 255000
TIGHT_QUIT_MS = 260000
SIMULATOR_STARTUP_ALLOWANCE_MS = 3000
TIGHT_SCREENSHOT_TRACE_MARGIN_MS = 100
DEFERRED_SCREENSHOT_MS = 16000
DEFERRED_BACK_REQUEST_MS = 40000
DEFERRED_SLEEP_AFTER_RECOVERY_MS = 47000
DEFERRED_QUIT_MS = 52000
_ACTIVITY_RE = re.compile(r"^\[(?P<elapsed>\d+)\].*\[ACT\] (?P<action>Entering|Exiting) activity: (?P<activity>\S+)")
_ROI_TRACE_RE = re.compile(
    r".*\[RSVP\]\s+diagnostic_roi\s+stage=(?P<stage>[a-z_]+)\s+"
    r"frame=(?P<frame>\d+)\s+x=(?P<x>-?\d+)\s+y=(?P<y>-?\d+)\s+"
    r"w=(?P<w>\d+)\s+h=(?P<h>\d+)\s+px=(?P<px>\d+)\s+py=(?P<py>\d+)\s+"
    r"pw=(?P<pw>\d+)\s+ph=(?P<ph>\d+)\s+t=(?P<t>\d+)"
)
_GROUP_RE = re.compile(r".*\[RSVP\]\s+group count=\d+\s+active=\d+\s+words=(?P<words>\S+)")


def _load_helper():
    spec = importlib.util.spec_from_file_location("rsvp_window_sim_helper", SIM_HELPER)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load simulator helper: {SIM_HELPER}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _read_bmp_pixels(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise RuntimeError(f"invalid screenshot: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    width, signed_height = struct.unpack_from("<ii", data, 18)
    planes, bits_per_pixel = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    bitfields = bits_per_pixel == 32 and compression == 3
    if (width <= 0 or signed_height == 0 or planes != 1 or (compression != 0 and not bitfields) or
            bits_per_pixel not in (1, 8, 24, 32)):
        raise RuntimeError(f"unsupported screenshot format: {path}")
    height = abs(signed_height)
    row_size = ((width * bits_per_pixel + 31) // 32) * 4
    if pixel_offset + row_size * height > len(data):
        raise RuntimeError(f"truncated screenshot: {path}")
    palette: list[tuple[int, int, int]] = []
    if bits_per_pixel <= 8:
        palette_count = struct.unpack_from("<I", data, 46)[0] or (1 << bits_per_pixel)
        palette_offset = 14 + dib_size
        if palette_offset + palette_count * 4 > pixel_offset:
            raise RuntimeError(f"invalid screenshot palette: {path}")
        palette = [tuple(reversed(data[palette_offset + index * 4:palette_offset + index * 4 + 3]))
                   for index in range(palette_count)]
    pixels: list[tuple[int, int, int]] = []
    masks = struct.unpack_from("<III", data, 54) if bitfields else (0, 0, 0)

    def bitfield(value: int, mask: int) -> int:
        shift = (mask & -mask).bit_length() - 1
        maximum = mask >> shift
        return ((value & mask) >> shift) * 255 // maximum

    for output_y in range(height):
        source_y = output_y if signed_height < 0 else height - 1 - output_y
        row = data[pixel_offset + source_y * row_size:pixel_offset + (source_y + 1) * row_size]
        for x in range(width):
            if bits_per_pixel == 32:
                if bitfields:
                    value = struct.unpack_from("<I", row, x * 4)[0]
                    pixels.append(tuple(bitfield(value, mask) for mask in masks))
                else:
                    blue, green, red = row[x * 4:x * 4 + 3]
                    pixels.append((red, green, blue))
            elif bits_per_pixel == 24:
                blue, green, red = row[x * 3:x * 3 + 3]
                pixels.append((red, green, blue))
            elif bits_per_pixel == 8:
                pixels.append(palette[row[x]])
            else:
                pixels.append(palette[(row[x // 8] >> (7 - x % 8)) & 1])
    return width, height, pixels


def _bmp_dimensions(path: Path) -> tuple[int, int]:
    width, height, _ = _read_bmp_pixels(path)
    return width, height


def _assert_window_pair(first: Path, second: Path, orientation: int) -> None:
    width, height, before = _read_bmp_pixels(first)
    next_width, next_height, after = _read_bmp_pixels(second)
    if (next_width, next_height) != (width, height):
        raise RuntimeError("window evidence screenshots changed dimensions")
    if len(set(before)) < 2 or len(set(after)) < 2:
        raise RuntimeError("window evidence screenshot is blank")

    changed_inside = 0
    changed_outside = 0
    dark_inside = 0
    for index, (old_pixel, new_pixel) in enumerate(zip(before, after)):
        x = index % width
        y = index // width
        # Simulator screenshots are already in logical orientation; the RSVP
        # line stays horizontal in all four settings.
        inside = height // 4 <= y < height * 3 // 4
        if inside and sum(new_pixel) < 720:
            dark_inside += 1
        if old_pixel != new_pixel:
            if inside:
                changed_inside += 1
            else:
                changed_outside += 1
    if changed_inside == 0:
        raise RuntimeError("window evidence did not change RSVP line pixels")
    if dark_inside == 0:
        raise RuntimeError("window evidence contains no non-white RSVP line pixels")
    if changed_outside != 0:
        raise RuntimeError(f"window evidence changed {changed_outside} pixels outside the mapped RSVP line band")


def _assert_probe_pair(full_size_ptl: Path, line_candidate: Path) -> None:
    """Ensure both awaited probe screens show a changing, visible RSVP word."""
    width, height, before = _read_bmp_pixels(full_size_ptl)
    next_width, next_height, after = _read_bmp_pixels(line_candidate)
    if (next_width, next_height) != (width, height):
        raise RuntimeError("probe evidence screenshots changed dimensions")
    if len(set(before)) < 2 or len(set(after)) < 2:
        raise RuntimeError("probe evidence screenshot is blank")

    changed_inside = 0
    dark_inside = 0
    for index, (old_pixel, new_pixel) in enumerate(zip(before, after)):
        y = index // width
        inside = height // 4 <= y < height * 3 // 4
        if inside and sum(new_pixel) < 720:
            dark_inside += 1
        if inside and old_pixel != new_pixel:
            changed_inside += 1
    if changed_inside == 0:
        raise RuntimeError("probe evidence did not show the required word transition")
    if dark_inside == 0:
        raise RuntimeError("probe evidence contains no non-white RSVP word pixels")


def _assert_tight_probe_screen(path: Path) -> None:
    """Require the displayed tight probe to contain the RSVP line content."""
    width, height, pixels = _read_bmp_pixels(path)
    line_top = height // 4
    line_bottom = height * 3 // 4
    if not any(
        sum(pixels[y * width + x]) < 720
        for y in range(line_top, line_bottom)
        for x in range(width)
    ):
        raise RuntimeError("tight probe has no dark content in the RSVP line ROI")


def _read_roi_trace(log: str) -> list[dict[str, int | str]]:
    entries: list[dict[str, int | str]] = []
    for line in log.splitlines():
        match = _ROI_TRACE_RE.match(line)
        if not match:
            continue
        entry: dict[str, int | str] = {"stage": match.group("stage")}
        for field in ("frame", "x", "y", "w", "h", "px", "py", "pw", "ph", "t"):
            entry[field] = int(match.group(field))
        entries.append(entry)
    if not entries:
        raise RuntimeError("simulator log has no successful diagnostic_roi rectangle evidence")
    return entries


def _assert_tight_probe_word(log: str, expected: str = "to") -> None:
    """Tie the tight probe rectangle to the deterministic expected fixture word."""
    last_words: str | None = None
    probe_words: list[str] = []
    for line in log.splitlines():
        group = _GROUP_RE.match(line)
        if group:
            last_words = group.group("words")
            continue
        marker = _ROI_TRACE_RE.match(line)
        if marker and marker.group("stage") == "tight_probe" and last_words is not None:
            probe_words.append(last_words)
    if not probe_words:
        raise RuntimeError("simulator log does not associate a word with the tight probe")
    if not any(expected in words.split("|") for words in probe_words):
        raise RuntimeError(
            f"tight probe word is not {expected!r}: " + ", ".join(probe_words)
        )


def _roi_entries_between(entries: list[dict[str, int | str]], start_ms: int, end_ms: int,
                         margin_ms: int = 5000) -> list[dict[str, int | str]]:
    selected = [
        entry for entry in entries
        if start_ms - margin_ms <= _roi_int(entry, "t") <= end_ms + margin_ms
    ]
    if not selected:
        raise RuntimeError(
            f"simulator log has no diagnostic_roi entries around {start_ms}..{end_ms}ms"
        )
    return selected


def _roi_int(entry: dict[str, int | str], field: str) -> int:
    value = entry[field]
    if not isinstance(value, int):
        raise RuntimeError(f"diagnostic_roi field {field!r} is not numeric")
    return value


def _roi_rect_for_screenshot(entry: dict[str, int | str], width: int, height: int) -> tuple[int, int, int, int]:
    """Map logical renderer coordinates into the screenshot's drawable pixels.

    SDL keeps the renderer in the panel's logical size, while macOS HiDPI
    screenshots use the larger drawable size. Small synthetic unit-test BMPs
    intentionally use coordinates directly and therefore stay unscaled.
    """
    if max(width, height) <= 800:
        logical_width, logical_height = width, height
    elif width > height:
        # The X3 simulator uses its physical 792x528 panel geometry in
        # landscape.  A Retina screenshot is exactly 2x these logical axes.
        logical_width, logical_height = X3_PANEL_WIDTH, X3_PANEL_HEIGHT
    else:
        # Portrait swaps the same panel axes; do not use the X4 480x800 size.
        logical_width, logical_height = X3_PANEL_HEIGHT, X3_PANEL_WIDTH
    x = _roi_int(entry, "x")
    y = _roi_int(entry, "y")
    right = x + _roi_int(entry, "w")
    bottom = y + _roi_int(entry, "h")
    return (
        max(0, math.floor(x * width / logical_width)),
        max(0, math.floor(y * height / logical_height)),
        min(width, math.ceil(right * width / logical_width)),
        min(height, math.ceil(bottom * height / logical_height)),
    )


def _roi_rect_contains(entry: dict[str, int | str], bbox: tuple[int, int, int, int],
                       width: int, height: int) -> bool:
    left, top, right, bottom = _roi_rect_for_screenshot(entry, width, height)
    return left <= bbox[0] and top <= bbox[1] and right > bbox[2] and bottom > bbox[3]


def _roi_union_contains(entries: list[dict[str, int | str]], bbox: tuple[int, int, int, int],
                        width: int, height: int, points: list[tuple[int, int]] | None = None) -> bool:
    """Check a changed bbox against the union of the logged logical rectangles."""
    rectangles = []
    for entry in entries:
        left, top, right, bottom = _roi_rect_for_screenshot(entry, width, height)
        if right > left and bottom > top:
            rectangles.append((left, top, right, bottom))
    if not rectangles:
        return False
    union_left = min(left for left, _, _, _ in rectangles)
    union_top = min(top for _, top, _, _ in rectangles)
    union_right = max(right for _, _, right, _ in rectangles)
    union_bottom = max(bottom for _, _, _, bottom in rectangles)
    if not (union_left <= bbox[0] and union_top <= bbox[1] and
            union_right > bbox[2] and union_bottom > bbox[3]):
        return False
    points_to_check = points or [
        (bbox[0], bbox[1]), (bbox[2], bbox[1]),
        (bbox[0], bbox[3]), (bbox[2], bbox[3]),
    ]
    return all(
        any(left <= x < right and top <= y < bottom for left, top, right, bottom in rectangles)
        for x, y in points_to_check
    )


def _dark_bbox(path: Path, *, line_only: bool,
               region: tuple[int, int, int, int] | None = None) -> tuple[int, int, int, int] | None:
    width, height, pixels = _read_bmp_pixels(path)
    left, top, right, bottom = region or (0, 0, width, height)
    if line_only:
        top = max(top, height // 4)
        bottom = min(bottom, height * 3 // 4)
    candidate_rows = range(top, bottom)
    if region is not None and width * height > 100000:
        # The probe screen draws two thin guide marks above and below the
        # word. They are useful visual cues but are outside the PTL payload;
        # retain rows with actual glyph density for the containment proof.
        candidate_rows = [
            y for y in candidate_rows
            if sum(sum(pixels[y * width + x]) < 720 for x in range(left, right)) >= 5
        ]
    points = [
        (x, y)
        for y in candidate_rows
        for x in range(left, right)
        if sum(pixels[y * width + x]) < 720
    ]
    if not points:
        return None
    return min(x for x, _ in points), min(y for _, y in points), max(x for x, _ in points), max(y for _, y in points)


def _assert_roi_trace(log: str, probe_path: Path, tight_row: dict[str, str],
                      window_row: dict[str, str]) -> tuple[list[dict[str, int | str]], list[dict[str, int | str]]]:
    """Check simulator PTL rectangles and return probe/measurement entries."""
    entries = _read_roi_trace(log)
    by_stage = {}
    for entry in entries:
        stage = entry["stage"]
        by_stage.setdefault(stage, []).append(entry)
        px, py, pw, ph = (_roi_int(entry, field) for field in ("px", "py", "pw", "ph"))
        x, y, w, h = (_roi_int(entry, field) for field in ("x", "y", "w", "h"))
        if (pw <= 0 or ph <= 0 or pw % 8 != 0 or
                px + pw > X3_PANEL_WIDTH or py + ph > X3_PANEL_HEIGHT):
            raise RuntimeError(f"invalid physical diagnostic_roi rectangle: {entry}")
        if w <= 0 or h <= 0 or x + w <= 0 or y + h <= 0:
            raise RuntimeError(f"invalid logical diagnostic_roi rectangle: {entry}")

    window_entries = by_stage.get("window_measurement", [])
    tight_probe_entries = by_stage.get("tight_probe", [])
    tight_entries = by_stage.get("tight_measurement", [])
    if not window_entries:
        raise RuntimeError("simulator log has no window_measurement ROI evidence")
    if not tight_probe_entries:
        raise RuntimeError("simulator log has no tight_probe ROI evidence")
    if not tight_entries:
        raise RuntimeError("simulator log has no tight_measurement ROI evidence")

    def check_row_metrics(row: dict[str, str], source: list[dict[str, int | str]], label: str) -> None:
        widths = [_roi_int(entry, "pw") for entry in source]
        heights = [_roi_int(entry, "ph") for entry in source]
        expected = {
            "roi_sample_count": len(source),
            "roi_min_width": min(widths),
            "roi_max_width": max(widths),
            "roi_min_height": min(heights),
            "roi_max_height": max(heights),
        }
        for field, value in expected.items():
            if int(row[field]) != value:
                raise RuntimeError(
                    f"{label} CSV {field}={row[field]!r} disagrees with simulator ROI trace ({value})"
                )

    check_row_metrics(window_row, window_entries, "window")
    check_row_metrics(tight_row, tight_entries, "tight")

    broad_area = min(_roi_int(entry, "pw") * _roi_int(entry, "ph") for entry in window_entries)
    if any(_roi_int(entry, "pw") * _roi_int(entry, "ph") >= broad_area for entry in tight_probe_entries + tight_entries):
        raise RuntimeError("tight PTL rectangle is not smaller than the broad window rectangle")

    probe_width, probe_height, _ = _read_bmp_pixels(probe_path)
    broad_rectangles = [
        _roi_rect_for_screenshot(entry, probe_width, probe_height)
        for entry in window_entries
    ]
    broad_region = (
        min(left for left, _, _, _ in broad_rectangles),
        min(top for _, top, _, _ in broad_rectangles),
        max(right for _, _, right, _ in broad_rectangles),
        max(bottom for _, _, _, bottom in broad_rectangles),
    )
    # Landscape screenshots place the status text inside the generic central
    # half of the drawable. Restrict the probe bbox to the broad measured line
    # ROI before checking that it fits inside the tighter probe rectangle.
    probe_bbox = _dark_bbox(probe_path, line_only=False, region=broad_region)
    if probe_bbox is None or not any(
        _roi_rect_contains(entry, probe_bbox, probe_width, probe_height)
        for entry in tight_probe_entries
    ):
        raise RuntimeError("tight probe pixels are outside every logged tight PTL rectangle")
    return tight_probe_entries, tight_entries


def _tight_roi_metric(row: dict[str, str], name: str) -> int:
    try:
        value = int(row[name])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f"tight ROI metric {name!r} is not an integer") from exc
    if value < 0:
        raise RuntimeError(f"tight ROI metric {name!r} is negative: {value}")
    return value


def _assert_tight_metrics(rows: list[dict[str, str]], *, complete: bool) -> None:
    """Validate shared confirmation and per-row tight ROI diagnostics."""
    if len(rows) != 3 or [row.get("variant") for row in rows] != [
        "full", "window", "tight_window"
    ]:
        raise RuntimeError("report variants must be full, window, tight_window")

    roi_fields = (
        "roi_sample_count",
        "roi_min_width",
        "roi_max_width",
        "roi_min_height",
        "roi_max_height",
    )
    expected_confirmation = "1" if complete else "0"
    for row in rows:
        if row.get("tight_probe_confirmed") != expected_confirmation:
            raise RuntimeError(
                "tight_probe_confirmed does not match the expected workflow state"
            )

    full = rows[0]
    if any(_tight_roi_metric(full, field) != 0 for field in roi_fields):
        raise RuntimeError("full row has tight ROI samples")

    def require_samples(row: dict[str, str], label: str) -> None:
        metrics = {field: _tight_roi_metric(row, field) for field in roi_fields}
        if metrics["roi_sample_count"] <= 0:
            raise RuntimeError(f"{label} branch has no ROI samples")
        if metrics["roi_min_width"] <= 0 or metrics["roi_min_height"] <= 0:
            raise RuntimeError(f"{label} ROI dimensions must be positive")
        if metrics["roi_max_width"] < metrics["roi_min_width"]:
            raise RuntimeError(f"{label} ROI max width is below its minimum")
        if metrics["roi_max_height"] < metrics["roi_min_height"]:
            raise RuntimeError(f"{label} ROI max height is below its minimum")
        if (metrics["roi_max_width"] > X3_PANEL_WIDTH or
                metrics["roi_max_height"] > X3_PANEL_HEIGHT):
            raise RuntimeError(f"{label} ROI exceeds the X3 physical display")

    # Window ROI samples are expected as soon as the window branch has
    # presented measured frames. Tight samples are only expected after its
    # independently confirmed probe; an incomplete run may end earlier.
    if complete:
        require_samples(rows[1], "window")
        require_samples(rows[2], "tight")
    elif any(_tight_roi_metric(rows[2], field) != 0 for field in roi_fields):
        raise RuntimeError("tight row has ROI samples before its probe completes")


def _assert_tight_pair(first: Path, second: Path, tight_row: dict[str, str],
                       roi_entries: list[dict[str, int | str]] | None = None) -> None:
    """Ensure consecutive tight frames change only inside the existing line ROI."""
    width, height, before = _read_bmp_pixels(first)
    next_width, next_height, after = _read_bmp_pixels(second)
    if (next_width, next_height) != (width, height):
        raise RuntimeError("tight evidence screenshots changed dimensions")

    changed: list[tuple[int, int]] = []
    dark_changed = False
    for index, (old_pixel, new_pixel) in enumerate(zip(before, after)):
        if old_pixel != new_pixel:
            x = index % width
            y = index // width
            changed.append((x, y))
            if sum(old_pixel) < 720 or sum(new_pixel) < 720:
                dark_changed = True
    if not changed:
        raise RuntimeError("tight evidence did not change the RSVP line pixels")
    if not dark_changed:
        raise RuntimeError("tight evidence has no dark changing content")

    line_top = height // 4
    line_bottom = height * 3 // 4
    outside = [(x, y) for x, y in changed if not line_top <= y < line_bottom]
    if outside:
        raise RuntimeError(
            "tight ROI changed pixels outside the existing RSVP line ROI "
            f"(first={outside[0]!r})"
        )

    if roi_entries:
        min_x = min(x for x, _ in changed)
        max_x = max(x for x, _ in changed)
        min_y = min(y for _, y in changed)
        max_y = max(y for _, y in changed)
        changed_bbox = (min_x, min_y, max_x, max_y)
        if not _roi_union_contains(roi_entries, changed_bbox, width, height, changed):
            raise RuntimeError("tight screenshot changed pixels outside the logged tight PTL rectangle union")

    min_x = min(x for x, _ in changed)
    max_x = max(x for x, _ in changed)
    min_y = min(y for _, y in changed)
    max_y = max(y for _, y in changed)
    changed_width = max_x - min_x + 1
    changed_height = max_y - min_y + 1
    max_width = _tight_roi_metric(tight_row, "roi_max_width")
    max_height = _tight_roi_metric(tight_row, "roi_max_height")
    if max_width <= 0 or max_height <= 0:
        raise RuntimeError("tight evidence has no reported physical ROI dimensions")
    # The CSV stores physical panel axes, while the screenshot is in the
    # selected logical orientation and may be HiDPI. Convert the two physical
    # extents into the screenshot's drawable axes before comparing them.
    if max(width, height) <= 800:
        screenshot_roi_width = max_width
        screenshot_roi_height = max_height
    elif width > height:
        screenshot_roi_width = max_width * width / X3_PANEL_WIDTH
        screenshot_roi_height = max_height * height / X3_PANEL_HEIGHT
    else:
        screenshot_roi_width = max_height * width / X3_PANEL_HEIGHT
        screenshot_roi_height = max_width * height / X3_PANEL_WIDTH
    if changed_width > math.ceil(screenshot_roi_width) or changed_height > math.ceil(screenshot_roi_height):
        raise RuntimeError(
            "tight screenshot change exceeds the reported physical ROI "
            f"({changed_width}x{changed_height} > "
            f"{math.ceil(screenshot_roi_width)}x{math.ceil(screenshot_roi_height)})"
        )


def _first_distinct_tight_pair(
    screenshots: list[Path], requested_times_ms: tuple[int, ...],
    tight_row: dict[str, str],
    roi_entries: list[dict[str, int | str]] | None = None,
) -> tuple[int, int, list[dict[str, int | str]] | None]:
    """Validate and return the first changing pair from uneven tight captures.

    The fixture intentionally runs uncapped, so a pair selected at a fixed
    offset can occasionally land on the same word in two screenshots. A
    no-change pair is therefore skipped; containment, dark-content, and ROI
    extent failures still fail the qualification immediately.
    """
    if len(screenshots) != len(requested_times_ms) or len(screenshots) < 2:
        raise RuntimeError("tight evidence needs at least two scheduled screenshots")
    for first_index, first in enumerate(screenshots[:-1]):
        for second_index in range(first_index + 1, len(screenshots)):
            pair_entries = None
            if roi_entries is not None:
                pair_entries = []
                for requested_ms in (requested_times_ms[first_index], requested_times_ms[second_index]):
                    capture_ms = requested_ms + SIMULATOR_STARTUP_ALLOWANCE_MS
                    pair_entries.extend(_roi_entries_between(
                        roi_entries, capture_ms, capture_ms,
                        margin_ms=TIGHT_SCREENSHOT_TRACE_MARGIN_MS,
                    ))
            try:
                _assert_tight_pair(first, screenshots[second_index], tight_row, pair_entries)
            except RuntimeError as exc:
                if "did not change the RSVP line pixels" not in str(exc):
                    raise
                continue
            return first_index, second_index, pair_entries
    raise RuntimeError("no changing screenshot pair in tight evidence")


def _assert_probe_confirmations(rows: list[dict[str, str]]) -> None:
    """Ensure the complete report records both operator-confirmed probes."""
    for row in rows:
        if (row.get("full_probe_confirmed") != "1" or
                row.get("line_probe_confirmed") != "1" or
                row.get("tight_probe_confirmed") != "1"):
            raise RuntimeError("complete report did not retain all visual probe confirmations")


def _assert_probe_failure(rows: list[dict[str, str]], scenario: str, error: str, error_code: str,
                          phase: str, operation: str, baseline_after: str,
                          full_probe_confirmed: str, line_probe_confirmed: str,
                          tight_probe_confirmed: str = "0") -> None:
    """Check a probe failure survives its recovery or remains latched for input gating."""
    # Probe failures happen before RsvpWindowBenchmark starts, so the branch
    # failure counter can remain zero while the diagnostic snapshot is valid.
    expected = {
        "last_operation": "full_recovery",
        "last_successful_operation": "full_recovery",
        "failure_phase": phase,
        "failure_operation": operation,
        "failure_requested": "window",
        "failure_actual": "none",
        "failure_error": error,
        # freeink::DisplayUpdateError is None=0, InvalidRegion=1,
        # BusyNotReady=2, BusyTimeout=3.
        "failure_error_code": error_code,
        "failure_duration_ms": "0",
        "failure_warmup_successes": "0",
        "failure_last_successful_operation": "full_resync",
        "diagnostic_stage": "error",
        "diagnostic_status": "stopped",
        "full_probe_confirmed": full_probe_confirmed,
        "line_probe_confirmed": line_probe_confirmed,
        "tight_probe_confirmed": tight_probe_confirmed,
        "trace_valid": "1",
        "trace_phase": "2",  # DisplayTracePhase::Window
        "trace_stage": "2",  # DisplayTraceStage::Preflight
        "trace_error": error_code,
        "trace_wait": "0",  # DisplayTraceWait::None
        "trace_busy_before": "0",
        "trace_busy_after": "0",
        "trace_baseline_before": "2",  # WindowBaselineState::Valid
        "trace_baseline_after": baseline_after,
        "trace_payload_bytes": "0",
        "trace_refresh_triggered": "0",
        "roi_sample_count": "0",
        "roi_min_width": "0",
        "roi_max_width": "0",
        "roi_min_height": "0",
        "roi_max_height": "0",
    }
    for row in rows:
        for field, value in expected.items():
            if row.get(field) != value:
                raise RuntimeError(
                    f"{scenario} scenario: {field}={row.get(field)!r}, expected {value!r}"
                )
        if (int(row["trace_width"]) <= 0 or int(row["trace_height"]) <= 0 or
                int(row["trace_width"]) > X3_PANEL_WIDTH or
                int(row["trace_height"]) > X3_PANEL_HEIGHT or
                int(row["trace_width"]) % 8 != 0):
            raise RuntimeError(f"{scenario} trace did not retain a valid physical candidate region")


def _assert_busy_not_ready_failure(rows: list[dict[str, str]]) -> None:
    _assert_probe_failure(rows, "BusyNotReady", "busy_not_ready", "2", "line_candidate", "line_candidate",
                          "2", "1", "0")


def _assert_busy_timeout_failure(rows: list[dict[str, str]]) -> None:
    _assert_probe_failure(rows, "BusyTimeout", "busy_timeout", "3", "full_size_ptl", "full_size_ptl", "0", "0",
                          "0")


def _assert_deferred_recovery(rows: list[dict[str, str]], log: str,
                              back_request_ms: int) -> None:
    """Prove the checked-display latch gates navigation until Back recovery.

    The simulator does not expose a separate timestamp for the successful
    checked recovery.  The report's retained full-recovery metadata, together
    with the activity boundary after the scripted Back request, is the
    available evidence for this ordering.
    """
    _assert_busy_timeout_failure(rows)
    activity_events = []
    for line in log.splitlines():
        match = _ACTIVITY_RE.match(line)
        if match:
            activity_events.append((int(match.group("elapsed")), match.group("action"),
                                    match.group("activity")))
    rsvp_entry = next((elapsed for elapsed, action, activity in activity_events
                       if action == "Entering" and activity == "RsvpReader"), None)
    if rsvp_entry is None:
        raise RuntimeError("timeout-deferred scenario did not enter RSVP reader")
    blocked_activities = {"Home", "Sleep", "EpubReader", "EpubReaderMenu"}
    escaped_while_pending = [event for event in activity_events
                             if rsvp_entry <= event[0] < back_request_ms and event[2] in blocked_activities]
    if escaped_while_pending:
        raise RuntimeError("Home or Sleep escaped while checked display recovery was pending")

    post_recovery_rsvp_exit = [elapsed for elapsed, action, activity in activity_events
                               if elapsed >= back_request_ms and action == "Exiting" and activity == "RsvpReader"]
    if not post_recovery_rsvp_exit:
        # Back is handled by the checked-failure branch before the normal RSVP
        # action logger, so the activity transition is the simulator's
        # observable boundary for this special recovery request.
        raise RuntimeError("timeout-deferred scenario did not leave RSVP reader after Back recovery")

    post_recovery_sleep = [elapsed for elapsed, action, activity in activity_events
                           if elapsed >= back_request_ms and action == "Entering" and activity == "Sleep"]
    if not post_recovery_sleep:
        raise RuntimeError("timeout-deferred scenario did not reach the intended Sleep after recovery")


def _read_report(path: Path, complete: bool, expected_orientation: str | None = None) -> list[dict[str, str]]:
    if not path.is_file():
        raise RuntimeError(f"missing CSV report: {path}")
    with path.open(newline="", encoding="ascii") as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
    expected_tail = [
        "tight_probe_confirmed", "roi_sample_count", "roi_min_width", "roi_max_width",
        "roi_min_height", "roi_max_height",
    ]
    if reader.fieldnames is None or reader.fieldnames[-len(expected_tail):] != expected_tail:
        raise RuntimeError("CSV tight diagnostics must be appended in the frozen field order")
    if [row.get("variant") for row in rows] != ["full", "window", "tight_window"]:
        raise RuntimeError("CSV must contain stable full/window/tight_window rows")
    required = {
        "controller", "confidence", "orientation", "power", "target_wpm", "elapsed_ms",
        "cpu_sample_count", "cpu_min_mhz", "cpu_max_mhz",
        "frame_count", "frame_min_ms", "frame_mean_ms", "frame_max_ms", "interval_count",
        "refresh_count", "actual_window_count", "actual_full_count", "fallback_count", "last_fallback",
        "failure_count", "last_operation", "last_successful_operation", "full_warmup_successes",
        "window_warmup_successes",
        "failure_phase", "failure_operation", "failure_requested", "failure_actual", "failure_error",
        "failure_error_code", "failure_duration_ms", "failure_warmup_successes",
        "failure_last_successful_operation",
        "firmware_version", "diagnostic_stage", "diagnostic_status", "full_probe_confirmed",
        "line_probe_confirmed", "tight_probe_confirmed", "roi_sample_count", "roi_min_width",
        "roi_max_width", "roi_min_height", "roi_max_height", "trace_valid", "trace_phase", "trace_stage",
        "trace_error", "trace_wait",
        "trace_busy_before", "trace_busy_after", "trace_baseline_before", "trace_baseline_after", "trace_x",
        "trace_y", "trace_width", "trace_height", "trace_payload_bytes", "trace_refresh_triggered",
    }
    if any(not required.issubset(row) for row in rows):
        raise RuntimeError("CSV report is missing required metadata or distributions")
    if expected_orientation is not None and any(row["orientation"] != expected_orientation for row in rows):
        raise RuntimeError(f"CSV orientation does not match requested {expected_orientation}")
    if any(row["target_wpm"] != "0" for row in rows):
        raise RuntimeError("max-speed diagnostic must report target_wpm=0 (unlimited pacing)")
    if complete:
        if any(int(row["elapsed_ms"]) < 60000 for row in rows):
            raise RuntimeError("all measured branches must last at least 60 seconds")
        if any(int(row["frame_count"]) == 0 for row in rows):
            raise RuntimeError("all measured branches must contain presented frames")
        if int(rows[1]["actual_window_count"]) == 0:
            raise RuntimeError("window branch did not report an actual window update")
        if int(rows[2]["actual_window_count"]) == 0:
            raise RuntimeError("tight branch did not report an actual window update")
    _assert_tight_metrics(rows, complete=complete)
    return rows


def _write_state(helper, run_root: Path, fixture: Path, orientation: int, language: str = "EN") -> None:
    helper.write_simulator_state(run_root, fixture, orientation, True, 14, 0, 100, False, False, language)
    settings_path = run_root / "fs_/.crosspoint/settings.json"
    before = json.loads(settings_path.read_text(encoding="utf-8"))
    before["screenInverted"] = 0
    settings_path.write_text(json.dumps(before), encoding="utf-8")


def _run(program: Path, helper, fixture: Path, output: Path, name: str, orientation: int, inputs: str,
         screenshots: dict[int, str], timeout: int, extra_env: dict[str, str] | None = None,
         block_report: bool = False, language: str = "EN", allow_timeout: bool = False) -> tuple[Path, str]:
    (output / f"{name}.log").unlink(missing_ok=True)
    (output / f"{name}.csv").unlink(missing_ok=True)
    for screenshot in screenshots.values():
        (output / screenshot).unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"crossrsvp-window-{name}-") as temp:
        run_root = Path(temp)
        _write_state(helper, run_root, fixture, orientation, language)
        if block_report:
            (run_root / REPORT).mkdir(parents=False)
        settings_before = (run_root / "fs_/.crosspoint/settings.json").read_bytes()
        env = os.environ.copy()
        env["CROSSPOINT_SIM_INPUT_SCRIPT"] = helper.delayed_inputs(inputs)
        env["CROSSPOINT_SIM_SCREENSHOTS"] = helper.screenshot_schedule(output, screenshots)
        env["CROSSPOINT_SIM_FREE_HEAP"] = "65536"
        env["CROSSPOINT_SIM_MAX_ALLOC_HEAP"] = "32768"
        if extra_env:
            env.update(extra_env)
        timed_out = False
        try:
            completed = subprocess.run([str(program)], cwd=run_root, env=env, text=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, timeout=timeout, check=False)
            simulator_output = completed.stdout
        except subprocess.TimeoutExpired as exc:
            if not allow_timeout:
                raise
            timed_out = True
            simulator_output = exc.stdout or ""
            if isinstance(simulator_output, bytes):
                simulator_output = simulator_output.decode("utf-8", errors="replace")
        (output / f"{name}.log").write_text(simulator_output, encoding="utf-8")
        if not timed_out and completed.returncode:
            raise RuntimeError(f"{name}: simulator exited {completed.returncode}")
        if (run_root / "fs_/.crosspoint/settings.json").read_bytes() != settings_before:
            raise RuntimeError(f"{name}: diagnostic changed persisted reader settings")
        report_copy = output / f"{name}.csv"
        if (run_root / REPORT).is_file():
            shutil.copy2(run_root / REPORT, report_copy)
        for screenshot in screenshots.values():
            path = output / screenshot
            if not path.is_file() or _bmp_dimensions(path)[0] == 0:
                raise RuntimeError(f"{name}: missing screenshot {path}")
        return report_copy, simulator_output


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--scenario", choices=("all", "complete", "abort", "timeout-recovery",
                                               "timeout-deferred", "report-failure", "busy-not-ready"),
                        default="all")
    parser.add_argument("--language", choices=("EN", "RU"), default="EN")
    parser.add_argument("--orientation", type=int, choices=ORIENTATIONS,
                        help="run one complete orientation (default: all four)")
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=1,
                        help="parallel complete-orientation simulator processes")
    args = parser.parse_args(argv)
    program = args.program.resolve()
    if not program.is_file():
        raise SystemExit(f"simulator program not found: {program}")
    args.output.mkdir(parents=True, exist_ok=True)
    helper = _load_helper()

    with tempfile.TemporaryDirectory(prefix="crossrsvp-window-fixture-") as temp:
        fixture = Path(temp) / "rsvp-window-test.epub"
        subprocess.run([sys.executable, str(FIXTURE_BUILDER), str(fixture), "--language", "en", "--words", "2400"],
                       cwd=REPO_ROOT, check=True)
        if args.scenario in ("all", "complete"):
            orientations = ORIENTATIONS if args.orientation is None else (args.orientation,)
            def run_orientation(orientation: int) -> None:
                name = f"complete-orientation-{orientation}"
                print(f"[simulator] starting {name}", flush=True)
                report, complete_log = _run(
                    program, helper, fixture, args.output.resolve(), name, orientation,
                    f"{START_RSVP};{START_DIAGNOSTIC};{FULL_SIZE_PTL_CONFIRM};{LINE_CANDIDATE_CONFIRM};"
                    f"{TIGHT_PROBE_CONFIRM};{TIGHT_QUIT_MS}:QUIT",
                    {8500: f"{name}-full-size-ptl.bmp", 14500: f"{name}-line-candidate.bmp",
                     28000: f"{name}-full-a.bmp", 30000: f"{name}-full-b.bmp",
                     95000: f"{name}-window-a.bmp", 97000: f"{name}-window-b.bmp",
                     TIGHT_PROBE_SCREENSHOT_MS: f"{name}-tight-probe.bmp",
                     **{requested_ms: f"{name}-tight-{index}.bmp"
                        for index, requested_ms in enumerate(TIGHT_MEASUREMENT_SCREENSHOTS_MS)},
                     TIGHT_RESULTS_MS: f"{name}-results.bmp"},
                    280, language=args.language,
                )
                rows = _read_report(report, complete=True, expected_orientation=ORIENTATION_NAMES[orientation])
                _assert_probe_confirmations(rows)
                _assert_probe_pair(args.output / f"{name}-full-size-ptl.bmp",
                                   args.output / f"{name}-line-candidate.bmp")
                _assert_window_pair(args.output / f"{name}-window-a.bmp",
                                    args.output / f"{name}-window-b.bmp", orientation)
                _assert_tight_probe_screen(args.output / f"{name}-tight-probe.bmp")
                _, tight_entries = _assert_roi_trace(
                    complete_log, args.output / f"{name}-tight-probe.bmp", rows[2], rows[1]
                )
                _assert_tight_probe_word(complete_log)
                tight_screenshots = [
                    args.output / f"{name}-tight-{index}.bmp"
                    for index in range(len(TIGHT_MEASUREMENT_SCREENSHOTS_MS))
                ]
                first_index, second_index, _ = _first_distinct_tight_pair(
                    tight_screenshots, TIGHT_MEASUREMENT_SCREENSHOTS_MS,
                    rows[2], tight_entries,
                )
                print(
                    f"[simulator] {name} tight pair indices "
                    f"{first_index}/{second_index} "
                    f"requested_ms={TIGHT_MEASUREMENT_SCREENSHOTS_MS[first_index]}/"
                    f"{TIGHT_MEASUREMENT_SCREENSHOTS_MS[second_index]}",
                    flush=True,
                )
                print(f"[simulator] passed {name}", flush=True)

            if args.jobs == 1:
                for orientation in orientations:
                    run_orientation(orientation)
            else:
                with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
                    list(executor.map(run_orientation, orientations))

        if args.scenario in ("all", "abort"):
            abort_report, _ = _run(
                program, helper, fixture, args.output.resolve(), "abort", 0,
                f"{START_RSVP};{START_DIAGNOSTIC};{FULL_SIZE_PTL_CONFIRM};{LINE_CANDIDATE_CONFIRM};36000:ENTER;42000:QUIT",
                {39000: "abort-results.bmp"}, 55,
                {"CROSSPOINT_SIM_DISPLAY_REFRESH_MS": "1000"},
                language=args.language,
            )
            _read_report(abort_report, complete=False, expected_orientation=ORIENTATION_NAMES[0])

        if args.scenario in ("all", "timeout-recovery"):
            timeout_report, _ = _run(
                program, helper, fixture, args.output.resolve(), "timeout-recovery", 0,
                f"{START_RSVP};{START_DIAGNOSTIC};{FULL_SIZE_PTL_CONFIRM};{LINE_CANDIDATE_CONFIRM};90000:ENTER;102000:QUIT",
                {96000: "timeout-recovery.bmp"}, 130,
                {"CROSSPOINT_SIM_WINDOW_FAILURE": "busy_timeout_once", "CROSSPOINT_SIM_WINDOW_RECOVER": "1"},
                language=args.language,
            )
            timeout_rows = _read_report(timeout_report, complete=False, expected_orientation=ORIENTATION_NAMES[0])
            _assert_busy_timeout_failure(timeout_rows)

        if args.scenario in ("all", "timeout-deferred"):
            deferred_report, deferred_log = _run(
                program, helper, fixture, args.output.resolve(), "timeout-deferred", 0,
                # A direct simulator SLEEP is a latched virtual power hold. Since
                # the input facade reports the longest held button, that latch
                # would make a later short BACK look like a long press and
                # suppress the recovery release. Use a short power-button
                # attempt while recovery is pending, then exercise the real
                # SLEEP shortcut only after checked recovery has completed.
                f"{START_RSVP};{START_DIAGNOSTIC};24000:P;{DEFERRED_BACK_REQUEST_MS}:BACK;"
                f"{DEFERRED_SLEEP_AFTER_RECOVERY_MS}:SLEEP;{DEFERRED_QUIT_MS}:QUIT",
                {DEFERRED_SCREENSHOT_MS: "timeout-deferred.bmp"}, 65,
                {"CROSSPOINT_SIM_WINDOW_FAILURE": "busy_timeout_once",
                 "CROSSPOINT_SIM_WINDOW_RECOVER": "1"}, language=args.language, allow_timeout=True,
            )
            deferred_rows = _read_report(deferred_report, complete=False,
                                         expected_orientation=ORIENTATION_NAMES[0])
            _assert_deferred_recovery(deferred_rows, deferred_log, DEFERRED_BACK_REQUEST_MS + 3000)

        if args.scenario in ("all", "report-failure"):
            _, report_failure_log = _run(
                program, helper, fixture, args.output.resolve(), "report-failure", 0,
                f"{START_RSVP};{START_DIAGNOSTIC};{FULL_SIZE_PTL_CONFIRM};{LINE_CANDIDATE_CONFIRM};30000:ENTER;34000:QUIT",
                {32000: "report-failure.bmp"}, 50,
                block_report=True, language=args.language,
            )
            if "RSVP-SPEED] sample" in report_failure_log:
                raise RuntimeError("window diagnostic emitted hot-path serial samples")

        if args.scenario in ("all", "busy-not-ready"):
            busy_report, _ = _run(
                program, helper, fixture, args.output.resolve(), "busy-not-ready", 0,
                f"{START_RSVP};{START_DIAGNOSTIC};{FULL_SIZE_PTL_CONFIRM};20000:ENTER;26000:QUIT",
                {23000: "busy-not-ready-results.bmp"}, 55,
                {"CROSSPOINT_SIM_WINDOW_FAILURE": "busy_not_ready_once",
                 "CROSSPOINT_SIM_WINDOW_RECOVER": "1",
                 # The first window call is the successful full-size PTL probe;
                 # inject BusyNotReady on the following line-candidate call.
                 "CROSSPOINT_SIM_WINDOW_FAILURE_AFTER": "1"},
                language=args.language,
            )
            busy_rows = _read_report(busy_report, complete=False, expected_orientation=ORIENTATION_NAMES[0])
            _assert_busy_not_ready_failure(busy_rows)

    print(f"RSVP window software validation passed; optical review still required; evidence: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
