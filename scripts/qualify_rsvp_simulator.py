#!/usr/bin/env python3

"""Run repeatable CrossRSVP scenarios in the X3, X4 and X4 Pro simulators."""

from __future__ import annotations

import argparse
import configparser
import filecmp
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
FIXTURE_SOURCE = REPO_ROOT / "test" / "rsvp" / "fixture"
FIXTURE_BUILDER = REPO_ROOT / "test" / "rsvp" / "build_fixture.py"
DEVICE_PROFILES = {
    "x3": ("Hardware detect: X3", (528, 792)),
    "x4": ("Hardware detect: X4", (480, 800)),
    "x4pro": ("Device: xteink_x4_pro", (480, 800)),
}


@dataclass(frozen=True)
class Scenario:
    name: str
    orientation: int
    input_script: str
    screenshots: dict[int, str]
    fatal_load: bool = False
    fixture_name: str = "default"
    quick_rsvp: bool = False
    start_home: bool = False
    rsvp_font_size: int = 14
    reader_menu_style: int = 0
    pace_wpm: int = 100
    short_word_grouping: bool = False
    refresh_latency_ms: int = 0
    input_script_after_wake: str = ""
    screenshots_after_wake: dict[int, str] | None = None
    expected_touch_actions: tuple[str, ...] = ()
    expected_controls: tuple[tuple[int, int], ...] = ()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", choices=DEVICE_PROFILES, default="x3")
    parser.add_argument(
        "--program",
        type=Path,
        help="built simulator executable (defaults to simulator_<device>)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="directory for evidence (defaults to the current device/version artifacts)",
    )
    parser.add_argument(
        "--scenario",
        action="append",
        help="run only the named scenario; repeat to select more than one",
    )
    return parser.parse_args()


def build_fixture(destination: Path) -> None:
    subprocess.run(
        [sys.executable, str(FIXTURE_BUILDER), str(FIXTURE_SOURCE), str(destination)],
        check=True,
        cwd=REPO_ROOT,
    )


def write_simulator_state(
    run_root: Path,
    fixture: Path,
    orientation: int,
    quick_rsvp: bool,
    rsvp_font_size: int,
    reader_menu_style: int,
    pace_wpm: int,
    short_word_grouping: bool,
    start_home: bool,
) -> None:
    fs_root = run_root / "fs_"
    books = fs_root / "books"
    crosspoint = fs_root / ".crosspoint"
    books.mkdir(parents=True)
    crosspoint.mkdir(parents=True)
    target = books / "rsvp-russian-qualification.epub"
    shutil.copy2(fixture, target)

    settings = {
        "embeddedStyle": 1,
        "language": "RU",
        "orientation": orientation,
        "rsvpFontSize": rsvp_font_size,
        "rsvpGuideStyle": 1,
        "rsvpPaceWpm": pace_wpm,
        "rsvpShortWordGrouping": int(short_word_grouping),
        "screenInverted": 1,
        "uiTheme": 1,
        "readerMenuStyle": reader_menu_style,
        "showReaderMenu": 1,
    }
    if quick_rsvp:
        settings["longPressMenuFunction"] = 5
    state = {"lastSleepFromReader": True, "showBootScreen": False}
    if not start_home:
        state["openEpubPath"] = "/books/rsvp-russian-qualification.epub"
    recent = {
        "books": [] if start_home else [
            {
                "author": "CrossRSVP",
                "coverBmpPath": "",
                "path": "/books/rsvp-russian-qualification.epub",
                "title": "RSVP Russian fixture",
            }
        ]
    }
    (crosspoint / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    (crosspoint / "state.json").write_text(json.dumps(state), encoding="utf-8")
    (crosspoint / "recent.json").write_text(json.dumps(recent), encoding="utf-8")


# Leave SDL/firmware startup time outside the scenario, including slow CI hosts.
STARTUP_ALLOWANCE_MS = 3000


def delayed_inputs(script: str) -> str:
    return ";".join(f"{int(at) + STARTUP_ALLOWANCE_MS}:{event}"
                    for at, event in (item.split(":", 1) for item in script.split(";") if item))


def screenshot_schedule(output: Path, screenshots: dict[int, str]) -> str:
    return ";".join(f"{at + STARTUP_ALLOWANCE_MS}:{output / filename}" for at, filename in screenshots.items())


def bmp_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    header = data[:26]
    if len(header) != 26 or header[:2] != b"BM":
        raise RuntimeError(f"invalid BMP screenshot: {path}")
    pixel_offset = struct.unpack_from("<I", header, 10)[0]
    pixels = data[pixel_offset:]
    if not pixels or len(set(pixels)) < 2:
        raise RuntimeError(f"blank BMP screenshot: {path}")
    return struct.unpack_from("<ii", header, 18)


def activity_entry_count(output: str, activity: str) -> int:
    marker = f"Entering activity: {activity}"
    return sum(line.endswith(marker) for line in output.splitlines())


def run_scenario(program: Path, fixtures: dict[str, Path], output: Path, scenario: Scenario, device: str) -> None:
    log_path = output / f"{scenario.name}.log"
    log_path.unlink(missing_ok=True)
    for filename in scenario.screenshots.values():
        (output / filename).unlink(missing_ok=True)
    for filename in (scenario.screenshots_after_wake or {}).values():
        (output / filename).unlink(missing_ok=True)

    with tempfile.TemporaryDirectory(prefix=f"crossrsvp-{scenario.name}-") as temp:
        run_root = Path(temp)
        write_simulator_state(
            run_root,
            fixtures[scenario.fixture_name],
            scenario.orientation,
            scenario.quick_rsvp,
            scenario.rsvp_font_size,
            scenario.reader_menu_style,
            scenario.pace_wpm,
            scenario.short_word_grouping,
            scenario.start_home,
        )
        env = os.environ.copy()
        env["CROSSPOINT_SIM_INPUT_SCRIPT"] = delayed_inputs(scenario.input_script)
        env["CROSSPOINT_SIM_SCREENSHOTS"] = screenshot_schedule(output, scenario.screenshots)
        env["CROSSPOINT_SIM_FREE_HEAP"] = "65536"
        env["CROSSPOINT_SIM_MAX_ALLOC_HEAP"] = "32768"
        if scenario.refresh_latency_ms:
            env["CROSSPOINT_SIM_DISPLAY_REFRESH_MS"] = str(scenario.refresh_latency_ms)
        if scenario.fatal_load:
            env["CROSSPOINT_SIM_RSVP_FATAL_LOAD"] = "1"
        if scenario.input_script_after_wake:
            env["CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE"] = delayed_inputs(scenario.input_script_after_wake)
        if scenario.screenshots_after_wake:
            env["CROSSPOINT_SIM_SCREENSHOTS_AFTER_WAKE"] = screenshot_schedule(
                output, scenario.screenshots_after_wake
            )

        completed = subprocess.run(
            [str(program)],
            cwd=run_root,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=45,
            check=False,
        )
        log_path.write_text(completed.stdout, encoding="utf-8")
        if completed.returncode != 0:
            raise RuntimeError(f"{scenario.name}: simulator exited {completed.returncode}; see {log_path}")
        marker, _ = DEVICE_PROFILES[device]
        if not any(line.endswith(marker) for line in completed.stdout.splitlines()):
            raise RuntimeError(f"{scenario.name}: {device} profile was not detected")
        if scenario.name != "focus-settings" and activity_entry_count(completed.stdout, "RsvpReader") == 0:
            raise RuntimeError(f"{scenario.name}: RSVP activity was not entered")
        if scenario.short_word_grouping and "short-word-grouping=on" not in completed.stdout:
            raise RuntimeError(f"{scenario.name}: Short-word grouping setting was not applied")
        if scenario.fatal_load:
            required = "Injected simulator RSVP source-open failure"
            if required not in completed.stdout or activity_entry_count(completed.stdout, "EpubReader") < 2:
                raise RuntimeError(f"{scenario.name}: fatal fallback was not observed")
        allowed_errors = {"[ERR] [EBP] Warning: Could not parse any TOC format"}
        if scenario.fatal_load:
            allowed_errors.add("[ERR] [RSVP] Injected simulator RSVP source-open failure")
        if scenario.name.startswith("touch-long-word-"):
            allowed_errors.add("[ERR] [RSVP] Failed to save RSVP checkpoint")
        unexpected_errors = [
            line for line in completed.stdout.splitlines() if "[ERR]" in line and not any(item in line for item in allowed_errors)
        ]
        if unexpected_errors:
            raise RuntimeError(f"{scenario.name}: unexpected error: {unexpected_errors[0]}; see {log_path}")
        if scenario.name == "flow-portrait":
            if activity_entry_count(completed.stdout, "RsvpReader") != 2:
                raise RuntimeError(f"{scenario.name}: RSVP re-entry was not observed")
            if activity_entry_count(completed.stdout, "EpubReader") < 2:
                raise RuntimeError(f"{scenario.name}: Paged fallback was not observed")
            if completed.stdout.count("[RSVP] refresh=fast") < 6:
                raise RuntimeError(f"{scenario.name}: playback did not produce the expected frames")
        if scenario.name == "boundary-image":
            if activity_entry_count(completed.stdout, "EpubReader") != 1:
                raise RuntimeError(f"{scenario.name}: PageForward unexpectedly entered Paged Mode")
        if scenario.name == "quick-entry":
            if activity_entry_count(completed.stdout, "RsvpReader") != 1:
                raise RuntimeError(f"{scenario.name}: long-Confirm did not enter RSVP exactly once")
            if activity_entry_count(completed.stdout, "EpubReaderMenu") != 0:
                raise RuntimeError(f"{scenario.name}: long-Confirm opened the reader menu")
        if scenario.name == "high-speed-controls":
            refresh_durations = []
            handled_controls = []
            for line in completed.stdout.splitlines():
                marker = "[RSVP] refresh=fast duration="
                if marker in line:
                    duration = line.split(marker, 1)[1].split("ms", 1)[0]
                    refresh_durations.append(int(duration))
                control_marker = "[RSVP] control action="
                if control_marker in line:
                    values = line.split(control_marker, 1)[1].split()
                    handled_controls.append(
                        (int(values[0]), int(values[1].split("=", 1)[1]))
                    )
            if not refresh_durations or min(refresh_durations) < scenario.refresh_latency_ms:
                raise RuntimeError(
                    f"{scenario.name}: refresh latency injection was not observed: {refresh_durations}"
                )
            expected_controls = [(1, 2), (1, 1), (1, 2), (6, 6)]
            if handled_controls != expected_controls:
                raise RuntimeError(
                    f"{scenario.name}: controls were not retained in order: {handled_controls}"
                )
            if activity_entry_count(completed.stdout, "RsvpReader") != 1:
                raise RuntimeError(f"{scenario.name}: RSVP activity was not entered exactly once")
            if activity_entry_count(completed.stdout, "EpubReader") < 2:
                raise RuntimeError(f"{scenario.name}: Back during refresh did not switch to Paged Mode")
        if scenario.name == "grouping-playback":
            frame_lines = [
                line for line in completed.stdout.splitlines() if "[RSVP] refresh=fast" in line
            ]
            if len(frame_lines) < 6:
                raise RuntimeError(
                    f"{scenario.name}: playback did not produce enough grouping frames: {len(frame_lines)}"
                )
            # The simulator injects a fixed heap value; it cannot qualify real
            # ESP32 heap stability. Check source membership instead.
            expected_groups = (
                "group count=1 active=0 words=Я||",
                "group count=3 active=1 words=не|сделал|бы",
                "group count=1 active=0 words=это,||",
                "group count=3 active=2 words=а|он|пришёл",
                "group count=2 active=1 words=в|дом.|",
            )
            cursor = 0
            for expected in expected_groups:
                found = completed.stdout.find(expected, cursor)
                if found < 0:
                    raise RuntimeError(f"{scenario.name}: missing ordered frame {expected!r}")
                cursor = found + len(expected)
        if scenario.name == "focus-settings":
            if activity_entry_count(completed.stdout, "Settings") < 1:
                raise RuntimeError(f"{scenario.name}: flat Settings activity was not entered")
            if activity_entry_count(completed.stdout, "TextSettings") < 1:
                raise RuntimeError(f"{scenario.name}: nested Text Settings activity was not entered")
            persisted = json.loads((run_root / "fs_" / ".crosspoint" / "settings.json").read_text(encoding="utf-8"))
            if persisted.get("focusReadingEnabled") != 0:
                raise RuntimeError(f"{scenario.name}: nested Focus Reading toggle did not persist")
        if scenario.name.startswith("orientation-"):
            if "group count=3 active=1 words=не|сделал|бы" not in completed.stdout:
                raise RuntimeError(f"{scenario.name}: three-word frame was not presented")
        if scenario.name == "grouping-resume":
            entries = completed.stdout.split("Entering activity: RsvpReader")
            if len(entries) != 3 or "group count=1 active=0 words=это,||" not in entries[-1]:
                raise RuntimeError("grouping-resume: re-entry did not continue after the whole group")
            if "group count=1 active=0 words=бы||" in entries[-1]:
                raise RuntimeError("grouping-resume: saved companion was replayed")
        if scenario.name.startswith("touch-"):
            actions = tuple(re.findall(r"touch_panel_action=(\w+)", completed.stdout))
            if actions != scenario.expected_touch_actions:
                raise RuntimeError(f"{scenario.name}: touch actions {actions}, expected {scenario.expected_touch_actions}")
            controls = tuple((int(action), int(state)) for action, state in
                             re.findall(r"control action=(\d+) state=(\d+)", completed.stdout))
            if scenario.expected_controls and controls != scenario.expected_controls:
                raise RuntimeError(f"{scenario.name}: controls {controls}, expected {scenario.expected_controls}")
            if scenario.name == "touch-settings" and activity_entry_count(completed.stdout, "Settings") != 1:
                raise RuntimeError("touch-settings: settings modal was not opened exactly once")
            if scenario.name == "touch-toolbar-entry":
                if activity_entry_count(completed.stdout, "EpubReaderMenu") != 0:
                    raise RuntimeError("touch-toolbar-entry: toolbar style opened the legacy reader menu")
                if activity_entry_count(completed.stdout, "RsvpReader") != 1:
                    raise RuntimeError("touch-toolbar-entry: More -> RSVP did not enter RSVP exactly once")
                if activity_entry_count(completed.stdout, "EpubReader") < 2:
                    raise RuntimeError("touch-toolbar-entry: Paged control did not return to EpubReader")
                if "touch_panel_geometry" not in completed.stdout:
                    raise RuntimeError("touch-toolbar-entry: RSVP paused control panel was not rendered")
            if scenario.name in ("touch-settings", "touch-frontlight"):
                if activity_entry_count(completed.stdout, "Home") or "Exiting activity: RsvpReader" in completed.stdout:
                    raise RuntimeError(f"{scenario.name}: modal dismissal did not return to the same RSVP session")
            if scenario.name in ("touch-settings", "touch-frontlight"):
                modal_name = "Settings" if scenario.name == "touch-settings" else "FrontlightPanel"
                before, during = completed.stdout.split(f"Entering activity: {modal_name}", 1)
                during, after = during.split(f"Exiting activity: {modal_name}", 1)
                groups = lambda text: re.findall(r"\[RSVP\] group [^\n]+", text)
                if groups(during) or not groups(after) or groups(before)[-1] != groups(after)[0]:
                    raise RuntimeError(f"{scenario.name}: modal changed the displayed reading position")
            if scenario.name == "touch-sleep-resume":
                entries = completed.stdout.split("Entering activity: RsvpReader")
                if len(entries) != 3 or "group count=1 active=0 words=это,||" not in entries[-1]:
                    raise RuntimeError("touch-sleep-resume: did not resume after the saved presentation group")
            if scenario.name == "touch-frontlight" and activity_entry_count(completed.stdout, "FrontlightPanel") != 1:
                raise RuntimeError("touch-frontlight: system modal was not opened exactly once")
            if scenario.name.startswith("touch-long-word-"):
                if "pause=oversized-word" not in completed.stdout and "Word does not fit" not in completed.stdout:
                    raise RuntimeError(f"{scenario.name}: oversized-word fallback was not observed")
                if activity_entry_count(completed.stdout, "EpubReader") < 2:
                    raise RuntimeError(f"{scenario.name}: Paged Mode was not opened from the fallback")
                if "touch_panel_action=play" in completed.stdout:
                    raise RuntimeError(f"{scenario.name}: fallback touch resumed playback unexpectedly")
        expected_pause = {
            "boundary-image": "pause=image",
            "boundary-chapter": "pause=chapter",
            "fatal-fallback": "pause=error",
        }.get(scenario.name[6:] if scenario.name.startswith("touch-") else scenario.name)
        if expected_pause and expected_pause not in completed.stdout:
            raise RuntimeError(f"{scenario.name}: expected diagnostic {expected_pause!r} was not observed")

        for filename in scenario.screenshots.values():
            path = output / filename
            if not path.is_file() or path.stat().st_size <= 54:
                raise RuntimeError(f"{scenario.name}: missing screenshot {path}")
            width, height = bmp_dimensions(path)
            if width <= 0 or height <= 0:
                raise RuntimeError(f"{scenario.name}: invalid screenshot dimensions {width}x{height}")
        for filename in (scenario.screenshots_after_wake or {}).values():
            path = output / filename
            if not path.is_file() or path.stat().st_size <= 54:
                raise RuntimeError(f"{scenario.name}: missing after-wake screenshot {path}")


def validate_flow_screenshots(output: Path) -> None:
    paused = output / "flow-portrait-paused.bmp"
    playing = output / "flow-portrait-playing.bmp"
    paused_again = output / "flow-portrait-paused-again.bmp"
    paged = output / "flow-portrait-paged-highlight.bmp"
    rsvp_again = output / "flow-portrait-rsvp-again.bmp"
    for left, right, relation in (
        (paused, playing, "different"),
        (playing, paused_again, "different"),
        (paged, paused_again, "different"),
        (paused_again, rsvp_again, "same"),
    ):
        identical = filecmp.cmp(left, right, shallow=False)
        if (relation == "same") != identical:
            raise RuntimeError(f"flow-portrait: expected {left.name} and {right.name} to be {relation}")


def validate_orientation_screenshots(output: Path, device: str, prefix: str = "orientation") -> None:
    paths = [output / f"{prefix}-{orientation}-paused.bmp" for orientation in range(4)]
    _, portrait = DEVICE_PROFILES[device]
    expected_dimensions = [portrait, portrait[::-1], portrait, portrait[::-1]]
    pixel_scale = None
    for path, expected in zip(paths, expected_dimensions):
        actual = bmp_dimensions(path)
        if actual[0] % expected[0] != 0 or actual[1] % expected[1] != 0:
            raise RuntimeError(f"{path.name}: expected scaled {device} geometry {expected}, got {actual}")
        current_scale = actual[0] // expected[0]
        if current_scale < 1 or actual[1] // expected[1] != current_scale:
            raise RuntimeError(f"{path.name}: expected uniform {device} pixel scale, got {actual}")
        if pixel_scale is None:
            pixel_scale = current_scale
        elif current_scale != pixel_scale:
            raise RuntimeError(f"{device} orientation screenshots use inconsistent pixel scales")
    if device != "x4pro" and (filecmp.cmp(paths[0], paths[2], shallow=False) or filecmp.cmp(paths[1], paths[3], shallow=False)):
        raise RuntimeError(f"opposite {device} orientations produced identical frames")


def guide_candidates(path: Path, logical_width: int, logical_height: int) -> set[tuple[int, int, int]]:
    """Find the two thin ORP guide strokes in the inverted fixture's word band."""
    data = path.read_bytes()
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    if bits != 32 or width % logical_width or height <= 0:
        raise RuntimeError(f"unsupported guide screenshot encoding: {path}")
    scale = width // logical_width
    offset = struct.unpack_from("<I", data, 10)[0]
    stride = width * 4
    candidates = set()
    for x in range(width // 2 - 10 * scale, width // 2 + 10 * scale):
        runs = []
        start = None
        for y in range(44 * scale, (logical_height - 300) * scale):
            pixel = offset + (height - 1 - y) * stride + x * 4
            white = min(data[pixel:pixel + 3]) > 150
            if white and start is None:
                start = y
            if not white and start is not None:
                if 12 * scale <= y - start <= 20 * scale:
                    runs.append(start)
                start = None
        for first in runs:
            for second in runs:
                if 50 * scale <= second - first <= 100 * scale:
                    candidates.add((x, first, second))
    return candidates


def validate_touch_guides(output: Path, orientation: int) -> None:
    width, height = (480, 800) if orientation % 2 == 0 else (800, 480)
    candidates = [guide_candidates(output / f"touch-orientation-{orientation}-{state}.bmp", width, height)
                  for state in ("paused", "playing", "paused-again")]
    if not set.intersection(*candidates):
        raise RuntimeError(f"touch-orientation-{orientation}: ORP guides moved or disappeared on Play/Pause")


def validate_boundary_skip_screenshots(output: Path) -> None:
    boundary = output / "boundary-image.bmp"
    skipped = output / "boundary-image-skipped.bmp"
    if filecmp.cmp(boundary, skipped, shallow=False):
        raise RuntimeError("boundary-image: PageForward did not replace the boundary prompt with the next word")


def validate_reopened_highlight(output: Path) -> None:
    after_exit = output / "highlight-after-exit.bmp"
    after_reopen = output / "highlight-after-reopen.bmp"
    if not filecmp.cmp(after_exit, after_reopen, shallow=False):
        raise RuntimeError("highlight-reopen: reopening Paged did not restore the active-word highlight")


def validate_short_word_grouping_screenshots(output: Path) -> None:
    grouping_off = output / "grouping-off.bmp"
    grouping_on = output / "grouping-playback.bmp"
    if filecmp.cmp(grouping_off, grouping_on, shallow=False):
        raise RuntimeError("short-word-grouping: enabled grouping did not change the rendered frame")


def scenarios() -> list[Scenario]:
    enter_rsvp = "1200:ENTER;2200:DOWN;2600:DOWN;3000:DOWN;3400:ENTER"
    return [
        Scenario(
            name="flow-portrait",
            orientation=0,
            input_script=(
                f"{enter_rsvp};5000:ENTER;6200:ENTER;7600:BACK;9000:ENTER;"
                "10000:DOWN;10400:DOWN;10800:DOWN;11200:ENTER;13000:QUIT"
            ),
            screenshots={
                4300: "flow-portrait-paused.bmp",
                5600: "flow-portrait-playing.bmp",
                6900: "flow-portrait-paused-again.bmp",
                8400: "flow-portrait-paged-highlight.bmp",
                12100: "flow-portrait-rsvp-again.bmp",
            },
        ),
        Scenario(
            name="boundary-image",
            orientation=0,
            input_script=f"{enter_rsvp};4600:ENTER;12000:DOWN;14000:QUIT",
            screenshots={11200: "boundary-image.bmp", 13000: "boundary-image-skipped.bmp"},
        ),
        Scenario(
            name="quick-entry",
            orientation=0,
            input_script="1200:ENTER:500;5000:QUIT",
            screenshots={4000: "quick-entry-paused.bmp"},
            quick_rsvp=True,
        ),
        Scenario(
            name="boundary-chapter",
            orientation=0,
            input_script=f"{enter_rsvp};4600:ENTER;10500:QUIT",
            screenshots={9300: "boundary-chapter.bmp"},
            fixture_name="chapter",
        ),
        Scenario(
            name="fatal-fallback",
            orientation=0,
            input_script=f"{enter_rsvp};6500:QUIT",
            screenshots={5200: "fatal-fallback-paged.bmp"},
            fatal_load=True,
        ),
        Scenario(
            name="high-speed-controls",
            orientation=0,
            input_script=(
                "3500:ENTER;4500:DOWN;5200:DOWN;5900:DOWN;6600:ENTER;"
                "7700:ENTER;8200:ENTER;9400:ENTER;9700:BACK;12000:QUIT"
            ),
            screenshots={},
            pace_wpm=240,
            refresh_latency_ms=450,
        ),
        Scenario(
            name="focus-settings",
            orientation=0,
            # Enable the direct Reader row, inspect Style, then disable it there
            # and return to Reader. Both paths must edit the same saved value.
            input_script=(
                "1200:DOWN;1600:DOWN;2000:DOWN;2400:ENTER;"
                "4200:ENTER;4600:DOWN;5000:DOWN;5400:DOWN;5600:ENTER;"
                "6000:UP;6400:UP;6800:ENTER;7400:ENTER;7800:ENTER;8200:ENTER;"
                "9600:DOWN;10000:ENTER;10600:BACK;11400:DOWN;11800:DOWN;13500:QUIT"
            ),
            screenshots={
                5500: "focus-reader.bmp",
                9400: "focus-style.bmp",
                10400: "focus-style-disabled.bmp",
                12600: "focus-reader-return.bmp",
            },
            start_home=True,
        ),
        Scenario(
            name="grouping-off",
            orientation=0,
            input_script=f"{enter_rsvp};4600:ENTER;11000:QUIT",
            screenshots={9600: "grouping-off.bmp"},
            fixture_name="grouping",
        ),
        Scenario(
            name="grouping-playback",
            orientation=0,
            input_script=f"{enter_rsvp};4600:ENTER;11000:QUIT",
            screenshots={9600: "grouping-playback.bmp"},
            fixture_name="grouping",
            short_word_grouping=True,
        ),
        Scenario(
            name="highlight-reopen",
            orientation=0,
            input_script=f"{enter_rsvp};5000:ENTER;6200:ENTER;7600:BACK;9000:SLEEP;11000:ENTER",
            screenshots={8400: "highlight-after-exit.bmp"},
            input_script_after_wake="5000:QUIT",
            screenshots_after_wake={3500: "highlight-after-reopen.bmp"},
        ),
        Scenario(
            name="grouping-resume",
            orientation=0,
            input_script=(
                f"{enter_rsvp};4100:DOWN;5000:BACK;6500:ENTER;"
                "7100:DOWN;7500:DOWN;7900:DOWN;8300:ENTER;10500:QUIT"
            ),
            screenshots={4600: "grouping-before-exit.bmp", 9500: "grouping-after-reentry.bmp"},
            fixture_name="grouping",
            short_word_grouping=True,
        ),
        *[
            Scenario(
                name=f"orientation-{orientation}",
                orientation=orientation,
                input_script=f"{enter_rsvp};4000:DOWN;5400:QUIT",
                screenshots={4500: f"orientation-{orientation}-paused.bmp"},
                fixture_name="grouping",
                short_word_grouping=True,
            )
            for orientation in range(4)
        ],
    ]


def touch_scenarios() -> list[Scenario]:
    # Logical pixels: Lyra list rows and the default UI scale, as verified in
    # touch-menu.bmp. The control panel is anchored to the oriented safe bottom.
    entry = "1200:TAP:0.5,0.5;2500:TAP:240,375"
    result = [
        Scenario("touch-toolbar-entry", 0,
                 "1200:TAP:0.5,0.5;2500:TAP:0.83,0.9;4000:TAP:0.5,0.60;6500:TAP:0.25,0.92;8000:QUIT",
                 {2200: "touch-toolbar-open.bmp", 3500: "touch-toolbar-more.bmp",
                  5200: "touch-toolbar-rsvp.bmp", 7300: "touch-toolbar-paged.bmp"},
                 reader_menu_style=1, expected_touch_actions=("paged",),
                 expected_controls=((6, 6),)),
        Scenario("touch-entry", 0, f"{entry};5000:QUIT",
                 {2000: "touch-menu.bmp", 4000: "touch-entry.bmp"}),
        Scenario("touch-controls", 0,
                 f"{entry};4100:TAP:360,546;5000:TAP:120,616;5900:TAP:120,686;"
                 "6800:TAP:360,686;7700:TAP:240,250;8600:TAP:120,546;"
                 "9000:TAP:120,546,240;10600:TAP:120,756;12500:QUIT",
                 {3600: "touch-controls-initial.bmp", 4600: "touch-controls-step.bmp",
                  5500: "touch-controls-rewind.bmp", 8200: "touch-controls-free-tap.bmp",
                  8850: "touch-controls-playing.bmp", 9600: "touch-controls-paused.bmp",
                  11400: "touch-controls-paged.bmp"},
                 expected_touch_actions=("step", "rewind5", "pace_down", "pace_up", "play", "paged"),
                 expected_controls=((2, 1), (3, 1), (4, 1), (5, 1), (1, 2), (1, 1), (6, 6))),
        Scenario("touch-settings", 0,
                 f"{entry};4100:TAP:360,616;6000:HOME;8000:QUIT",
                 {3600: "touch-settings-before.bmp", 5200: "touch-settings-open.bmp",
                  7100: "touch-settings-return.bmp"}, expected_touch_actions=("settings",)),
        Scenario("touch-frontlight", 0,
                 f"{entry};4500:TAP:120,546;4800:SWIPE:240,5,240,200,160;"
                 "6500:HOME;8500:QUIT",
                 {3800: "touch-frontlight-before.bmp", 5700: "touch-frontlight-open.bmp",
                  7500: "touch-frontlight-return.bmp"},
                 expected_touch_actions=("play",), expected_controls=((1, 2),)),
        Scenario("touch-refresh-input", 0,
                 "3500:TAP:240,400;6000:TAP:240,375;8000:TAP:120,546;"
                 "8300:TAP:120,546,240;10500:QUIT",
                 {9600: "touch-refresh-paused.bmp"}, refresh_latency_ms=450,
                 expected_touch_actions=("play",), expected_controls=((1, 2), (1, 1))),
    ]
    entry = "1200:TAP:0.5,0.5;2500:TAP:240,375"
    result.extend([
        Scenario("touch-boundary-image", 0, f"{entry};4600:TAP:120,546;12000:TAP:360,546;14000:QUIT",
                 {11200: "touch-boundary-image.bmp", 13000: "touch-boundary-image-skipped.bmp"},
                 expected_touch_actions=("play", "step")),
        Scenario("touch-boundary-chapter", 0, f"{entry};4600:TAP:120,546;10500:QUIT",
                 {9300: "touch-boundary-chapter.bmp"}, fixture_name="chapter", expected_touch_actions=("play",)),
        Scenario("touch-fatal-fallback", 0, f"{entry};6500:QUIT",
                 {5200: "touch-fatal-fallback.bmp"}, fatal_load=True),
        Scenario("touch-sleep-resume", 0, f"{entry};4000:TAP:360,546;6000:SLEEP;8000:ENTER",
                 {4800: "touch-sleep-before.bmp"}, fixture_name="grouping", short_word_grouping=True,
                 input_script_after_wake=f"{entry};5500:QUIT",
                 screenshots_after_wake={4500: "touch-sleep-after.bmp"}, expected_touch_actions=("step",)),
    ])
    for orientation in range(4):
        width, height = (480, 800) if orientation % 2 == 0 else (800, 480)
        entry = f"1200:TAP:0.5,0.5;2500:TAP:0.5,{375 / height:.6f}"
        control_y = (height - 254) / height
        pause_contact_ms = 80 if orientation == 0 else 240
        result.append(Scenario(f"touch-orientation-{orientation}", orientation,
            f"{entry};3600:TAP:0.75,{control_y:.6f};4800:TAP:0.25,{control_y:.6f};"
            f"5200:TAP:0.25,{control_y:.6f},{pause_contact_ms};7000:QUIT",
            {4200: f"touch-orientation-{orientation}-paused.bmp",
             5050: f"touch-orientation-{orientation}-playing.bmp",
             6200: f"touch-orientation-{orientation}-paused-again.bmp"},
            fixture_name="grouping", short_word_grouping=True,
            expected_touch_actions=("step", "play"), expected_controls=((2, 1), (1, 2), (1, 1))))
    # X4 Pro font bounds: CrossPointSettings::RSVP_FONT_SIZE_MIN/MAX are 12/18.
    # These scenarios exercise both orientations and both bounds with the same
    # panel flow; screenshot dimensions are checked by run_scenario, while glyph
    # fit/legibility remains a visual review item for the captured evidence.
    for font_size in (12, 18):
        for orientation in range(4):
            width, height = (480, 800) if orientation % 2 == 0 else (800, 480)
            entry = f"1200:TAP:0.5,0.5;2500:TAP:0.5,{375 / height:.6f}"
            # Default X4 Pro touch theme uses four 66px rows and a 292px
            # panel band; these values are also emitted by touch_panel_button
            # diagnostics for visual review.
            panel_top = height - 292
            play_y = (panel_top + 33) / height
            step_x = 0.75
            step_y = play_y
            result.append(Scenario(
                f"touch-font-{font_size}-orientation-{orientation}", orientation,
                f"{entry};3600:TAP:{step_x},{step_y:.6f};4800:TAP:0.25,{play_y:.6f};"
                f"5200:TAP:0.25,{play_y:.6f},240;7000:QUIT",
                {4200: f"touch-font-{font_size}-orientation-{orientation}-paused.bmp",
                 5050: f"touch-font-{font_size}-orientation-{orientation}-playing.bmp",
                 6200: f"touch-font-{font_size}-orientation-{orientation}-paused-again.bmp"},
                fixture_name="grouping", short_word_grouping=True, rsvp_font_size=font_size,
                expected_touch_actions=("step", "play"), expected_controls=((2, 1), (1, 2), (1, 1))))
    # The oversized token remains within the RSVP byte limit but cannot fit the
    # largest configured font. It must stop on the explicit Paged control.
    for orientation in (0, 1):
        width, height = (480, 800) if orientation % 2 == 0 else (800, 480)
        entry = f"1200:TAP:0.5,0.5;2500:TAP:0.5,{375 / height:.6f}"
        panel_top = height - 292
        paged_y = (panel_top + 4 * 66 - 33) / height
        result.append(Scenario(
            f"touch-long-word-orientation-{orientation}", orientation,
            f"{entry};5000:TAP:0.25,{(panel_top + 33) / height:.6f};"
            f"5500:TAP:0.75,{(panel_top + 33) / height:.6f};"
            f"6000:TAP:0.25,{paged_y:.6f};8000:QUIT",
            {4200: f"touch-long-word-orientation-{orientation}-fallback.bmp",
             6500: f"touch-long-word-orientation-{orientation}-paged.bmp"},
            fixture_name="long-word", rsvp_font_size=18,
            expected_touch_actions=("paged",), expected_controls=((9, 3), (6, 6))))
    return result


def main() -> int:
    args = parse_args()
    config = configparser.ConfigParser()
    config.read(REPO_ROOT / "platformio.ini")
    version = config.get("crossrsvp", "version")
    program = (args.program or REPO_ROOT / ".pio" / "build" / f"simulator_{args.device}" / "program").resolve()
    output = (args.output or REPO_ROOT / "artifacts" / f"crossrsvp-{args.device}-v{version}" / "simulator").resolve()
    if not program.is_file():
        raise SystemExit(f"simulator program not found: {program}; run `pio run -e simulator_{args.device}` first")
    output.mkdir(parents=True, exist_ok=True)
    summary_path = output / "summary.json"
    summary_path.unlink(missing_ok=True)

    with tempfile.TemporaryDirectory(prefix="crossrsvp-fixture-") as temp:
        fixture_root = Path(temp)
        fixture = fixture_root / "rsvp-russian-qualification.epub"
        build_fixture(fixture)
        chapter_source = fixture_root / "chapter-source"
        shutil.copytree(FIXTURE_SOURCE, chapter_source)
        (chapter_source / "OEBPS" / "chapter.xhtml").write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<html xmlns="http://www.w3.org/1999/xhtml" lang="ru">'
            "<head><title>RSVP chapter boundary fixture</title></head>"
            "<body><p>Первое слово главы.</p></body></html>\n",
            encoding="utf-8",
        )
        chapter_fixture = fixture_root / "rsvp-russian-chapter-boundary.epub"
        subprocess.run(
            [sys.executable, str(FIXTURE_BUILDER), str(chapter_source), str(chapter_fixture)],
            check=True,
            cwd=REPO_ROOT,
        )
        grouping_source = fixture_root / "grouping-source"
        shutil.copytree(FIXTURE_SOURCE, grouping_source)
        (grouping_source / "OEBPS" / "chapter.xhtml").write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<html xmlns="http://www.w3.org/1999/xhtml" lang="ru">'
            "<head><title>RSVP short-word grouping fixture</title></head>"
            "<body><p>Я не сделал бы это, а он пришёл в дом.</p></body></html>\n",
            encoding="utf-8",
        )
        grouping_fixture = fixture_root / "rsvp-russian-short-word-grouping.epub"
        subprocess.run(
            [sys.executable, str(FIXTURE_BUILDER), str(grouping_source), str(grouping_fixture)],
            check=True,
            cwd=REPO_ROOT,
        )
        long_word_source = shutil.copytree(FIXTURE_SOURCE, fixture_root / "long-word-source")
        long_word = "Ж" * 95  # 190 UTF-8 bytes: one token, below MAX_TOKEN_BYTES (200).
        (long_word_source / "OEBPS" / "chapter.xhtml").write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<html xmlns="http://www.w3.org/1999/xhtml" lang="ru">'
            "<head><title>RSVP long word fixture</title></head>"
            f"<body><p>{long_word} после длинного слова.</p></body></html>\n",
            encoding="utf-8",
        )
        long_word_fixture = fixture_root / "rsvp-russian-long-word.epub"
        subprocess.run(
            [sys.executable, str(FIXTURE_BUILDER), str(long_word_source), str(long_word_fixture)],
            check=True,
            cwd=REPO_ROOT,
        )
        fixtures = {
            "default": fixture,
            "chapter": chapter_fixture,
            "grouping": grouping_fixture,
            "long-word": long_word_fixture,
        }
        available = touch_scenarios() if args.device == "x4pro" else scenarios()
        selected = [scenario for scenario in available if not args.scenario or scenario.name in args.scenario]
        unknown = set(args.scenario or ()) - {scenario.name for scenario in available}
        if unknown:
            raise SystemExit(f"unknown scenario(s): {', '.join(sorted(unknown))}")
        for scenario in selected:
            print(f"[simulator] {scenario.name}", flush=True)
            run_scenario(program, fixtures, output, scenario, args.device)
        if any(scenario.name == "flow-portrait" for scenario in selected):
            validate_flow_screenshots(output)
        if any(scenario.name == "boundary-image" for scenario in selected):
            validate_boundary_skip_screenshots(output)
        if any(scenario.name == "highlight-reopen" for scenario in selected):
            validate_reopened_highlight(output)
        if {scenario.name for scenario in selected}.issuperset({"grouping-off", "grouping-playback"}):
            validate_short_word_grouping_screenshots(output)
        if {scenario.name for scenario in selected}.issuperset({f"orientation-{orientation}" for orientation in range(4)}):
            validate_orientation_screenshots(output, args.device)
        if {scenario.name for scenario in selected}.issuperset({f"touch-orientation-{orientation}" for orientation in range(4)}):
            validate_orientation_screenshots(output, args.device, "touch-orientation")
        for orientation in range(4):
            if any(scenario.name == f"touch-orientation-{orientation}" for scenario in selected):
                validate_touch_guides(output, orientation)
        for font_size in (12, 18):
            prefix = f"touch-font-{font_size}-orientation"
            if {scenario.name for scenario in selected}.issuperset({f"{prefix}-{orientation}" for orientation in range(4)}):
                validate_orientation_screenshots(output, args.device, prefix)

    summary_path.write_text(json.dumps({
        "device": args.device,
        "version": version,
        "program_sha256": hashlib.sha256(program.read_bytes()).hexdigest(),
        "scenarios": [scenario.name for scenario in selected],
        "hardware_qualified": False,
        "heap_values": "injected simulator constants, not measurements",
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Simulator qualification passed; evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
