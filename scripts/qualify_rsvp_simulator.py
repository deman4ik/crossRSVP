#!/usr/bin/env python3

"""Run repeatable CrossRSVP v0.1 scenarios in the X3 simulator."""

from __future__ import annotations

import argparse
import filecmp
import json
import os
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


@dataclass(frozen=True)
class Scenario:
    name: str
    orientation: int
    input_script: str
    screenshots: dict[int, str]
    fatal_load: bool = False
    fixture_name: str = "default"
    quick_rsvp: bool = False
    pace_wpm: int = 100
    refresh_latency_ms: int = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--program",
        type=Path,
        default=REPO_ROOT / ".pio" / "build" / "simulator_x3" / "program",
        help="built simulator_x3 executable",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "artifacts" / "rsvp-x3-v0.1" / "simulator",
        help="directory for logs and BMP screenshots",
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
    run_root: Path, fixture: Path, orientation: int, quick_rsvp: bool, pace_wpm: int
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
        "rsvpFontSize": 14,
        "rsvpGuideStyle": 1,
        "rsvpPaceWpm": pace_wpm,
        "screenInverted": 1,
        "uiTheme": 1,
    }
    if quick_rsvp:
        settings["longPressMenuFunction"] = 5
    state = {
        "lastSleepFromReader": True,
        "openEpubPath": "/books/rsvp-russian-qualification.epub",
        "showBootScreen": False,
    }
    recent = {
        "books": [
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


def screenshot_schedule(output: Path, screenshots: dict[int, str]) -> str:
    return ";".join(f"{at}:{output / filename}" for at, filename in screenshots.items())


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


def run_scenario(program: Path, fixtures: dict[str, Path], output: Path, scenario: Scenario) -> None:
    log_path = output / f"{scenario.name}.log"
    log_path.unlink(missing_ok=True)
    for filename in scenario.screenshots.values():
        (output / filename).unlink(missing_ok=True)

    with tempfile.TemporaryDirectory(prefix=f"crossrsvp-{scenario.name}-") as temp:
        run_root = Path(temp)
        write_simulator_state(
            run_root,
            fixtures[scenario.fixture_name],
            scenario.orientation,
            scenario.quick_rsvp,
            scenario.pace_wpm,
        )
        env = os.environ.copy()
        env["CROSSPOINT_SIM_INPUT_SCRIPT"] = scenario.input_script
        env["CROSSPOINT_SIM_SCREENSHOTS"] = screenshot_schedule(output, scenario.screenshots)
        env["CROSSPOINT_SIM_FREE_HEAP"] = "65536"
        env["CROSSPOINT_SIM_MAX_ALLOC_HEAP"] = "32768"
        if scenario.refresh_latency_ms:
            env["CROSSPOINT_SIM_DISPLAY_REFRESH_MS"] = str(scenario.refresh_latency_ms)
        if scenario.fatal_load:
            env["CROSSPOINT_SIM_RSVP_FATAL_LOAD"] = "1"

        completed = subprocess.run(
            [str(program)],
            cwd=run_root,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=30,
            check=False,
        )
        log_path.write_text(completed.stdout, encoding="utf-8")
        if completed.returncode != 0:
            raise RuntimeError(f"{scenario.name}: simulator exited {completed.returncode}; see {log_path}")
        if "Hardware detect: X3" not in completed.stdout:
            raise RuntimeError(f"{scenario.name}: X3 profile was not detected")
        if activity_entry_count(completed.stdout, "RsvpReader") == 0:
            raise RuntimeError(f"{scenario.name}: RSVP activity was not entered")
        if scenario.fatal_load:
            required = "Injected simulator RSVP source-open failure"
            if required not in completed.stdout or activity_entry_count(completed.stdout, "EpubReader") < 2:
                raise RuntimeError(f"{scenario.name}: fatal fallback was not observed")
        allowed_errors = {"[ERR] [EBP] Warning: Could not parse any TOC format"}
        if scenario.fatal_load:
            allowed_errors.add("[ERR] [RSVP] Injected simulator RSVP source-open failure")
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
        expected_pause = {
            "boundary-image": "pause=image",
            "boundary-chapter": "pause=chapter",
            "fatal-fallback": "pause=error",
        }.get(scenario.name)
        if expected_pause and expected_pause not in completed.stdout:
            raise RuntimeError(f"{scenario.name}: expected diagnostic {expected_pause!r} was not observed")

        for filename in scenario.screenshots.values():
            path = output / filename
            if not path.is_file() or path.stat().st_size <= 54:
                raise RuntimeError(f"{scenario.name}: missing screenshot {path}")
            width, height = bmp_dimensions(path)
            if width <= 0 or height <= 0:
                raise RuntimeError(f"{scenario.name}: invalid screenshot dimensions {width}x{height}")


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


def validate_orientation_screenshots(output: Path) -> None:
    paths = [output / f"orientation-{orientation}-paused.bmp" for orientation in range(4)]
    expected_dimensions = [(1056, 1584), (1584, 1056), (1056, 1584), (1584, 1056)]
    for path, expected in zip(paths, expected_dimensions):
        actual = bmp_dimensions(path)
        if actual != expected:
            raise RuntimeError(f"{path.name}: expected X3 orientation geometry {expected}, got {actual}")
    if filecmp.cmp(paths[0], paths[2], shallow=False) or filecmp.cmp(paths[1], paths[3], shallow=False):
        raise RuntimeError("opposite X3 orientations produced identical frames")


def validate_boundary_skip_screenshots(output: Path) -> None:
    boundary = output / "boundary-image.bmp"
    skipped = output / "boundary-image-skipped.bmp"
    if filecmp.cmp(boundary, skipped, shallow=False):
        raise RuntimeError("boundary-image: PageForward did not replace the boundary prompt with the next word")


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
                f"{enter_rsvp};3900:ENTER;4400:ENTER;5600:ENTER;5900:BACK;7600:QUIT"
            ),
            screenshots={},
            pace_wpm=240,
            refresh_latency_ms=450,
        ),
        *[
            Scenario(
                name=f"orientation-{orientation}",
                orientation=orientation,
                input_script=f"{enter_rsvp};5400:QUIT",
                screenshots={4500: f"orientation-{orientation}-paused.bmp"},
            )
            for orientation in range(4)
        ],
    ]


def main() -> int:
    args = parse_args()
    program = args.program.resolve()
    output = args.output.resolve()
    if not program.is_file():
        raise SystemExit(f"simulator program not found: {program}; run `pio run -e simulator_x3` first")
    output.mkdir(parents=True, exist_ok=True)

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
        fixtures = {"default": fixture, "chapter": chapter_fixture}
        available = scenarios()
        selected = [scenario for scenario in available if not args.scenario or scenario.name in args.scenario]
        unknown = set(args.scenario or ()) - {scenario.name for scenario in available}
        if unknown:
            raise SystemExit(f"unknown scenario(s): {', '.join(sorted(unknown))}")
        for scenario in selected:
            print(f"[simulator] {scenario.name}", flush=True)
            run_scenario(program, fixtures, output, scenario)
        if any(scenario.name == "flow-portrait" for scenario in selected):
            validate_flow_screenshots(output)
        if any(scenario.name == "boundary-image" for scenario in selected):
            validate_boundary_skip_screenshots(output)
        if {scenario.name for scenario in selected}.issuperset({f"orientation-{orientation}" for orientation in range(4)}):
            validate_orientation_screenshots(output)

    print(f"Simulator qualification passed; evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
