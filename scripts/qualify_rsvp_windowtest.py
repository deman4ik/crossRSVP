#!/usr/bin/env python3
"""Exercise the autonomous X3 RSVP window A/B diagnostic in the simulator."""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import importlib.util
import json
import os
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
REPORT = Path("fs_/.crosspoint/rsvp-window-test.csv")
START_RSVP = "1200:ENTER:900"
START_AB = "3600:ENTER:900"


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


def _read_report(path: Path, complete: bool, expected_orientation: str | None = None) -> list[dict[str, str]]:
    if not path.is_file():
        raise RuntimeError(f"missing CSV report: {path}")
    with path.open(newline="", encoding="ascii") as stream:
        rows = list(csv.DictReader(stream))
    if [row.get("variant") for row in rows] != ["full", "window"]:
        raise RuntimeError("CSV must contain stable full/window rows")
    required = {
        "controller", "confidence", "orientation", "power", "target_wpm", "elapsed_ms",
        "cpu_sample_count", "cpu_min_mhz", "cpu_max_mhz",
        "frame_count", "frame_min_ms", "frame_mean_ms", "frame_max_ms", "interval_count",
        "refresh_count", "actual_window_count", "actual_full_count", "fallback_count", "last_fallback",
        "failure_count",
    }
    if any(not required.issubset(row) for row in rows):
        raise RuntimeError("CSV report is missing required metadata or distributions")
    if expected_orientation is not None and any(row["orientation"] != expected_orientation for row in rows):
        raise RuntimeError(f"CSV orientation does not match requested {expected_orientation}")
    if complete:
        if any(int(row["elapsed_ms"]) < 60000 for row in rows):
            raise RuntimeError("both measured branches must last at least 60 seconds")
        if any(int(row["frame_count"]) == 0 for row in rows):
            raise RuntimeError("both measured branches must contain presented frames")
        if int(rows[1]["actual_window_count"]) == 0:
            raise RuntimeError("window branch did not report an actual window update")
    return rows


def _write_state(helper, run_root: Path, fixture: Path, orientation: int, language: str = "EN") -> None:
    helper.write_simulator_state(run_root, fixture, orientation, True, 14, 0, 100, False, False, language)
    settings_path = run_root / "fs_/.crosspoint/settings.json"
    before = json.loads(settings_path.read_text(encoding="utf-8"))
    before["screenInverted"] = 0
    settings_path.write_text(json.dumps(before), encoding="utf-8")


def _run(program: Path, helper, fixture: Path, output: Path, name: str, orientation: int, inputs: str,
         screenshots: dict[int, str], timeout: int, extra_env: dict[str, str] | None = None,
         block_report: bool = False, language: str = "EN") -> tuple[Path, str]:
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
        completed = subprocess.run([str(program)], cwd=run_root, env=env, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=timeout, check=False)
        (output / f"{name}.log").write_text(completed.stdout, encoding="utf-8")
        if completed.returncode:
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
        return report_copy, completed.stdout


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--scenario", choices=("all", "complete", "abort", "timeout-recovery",
                                               "timeout-deferred", "report-failure"),
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
                report, _ = _run(
                    program, helper, fixture, args.output.resolve(), name, orientation,
                    f"{START_RSVP};{START_AB};145000:QUIT",
                    {16000: f"{name}-full-a.bmp", 18000: f"{name}-full-b.bmp",
                     81000: f"{name}-window-a.bmp", 83000: f"{name}-window-b.bmp",
                     138000: f"{name}-results.bmp"},
                    165, language=args.language,
                )
                _read_report(report, complete=True, expected_orientation=ORIENTATION_NAMES[orientation])
                _assert_window_pair(args.output / f"{name}-window-a.bmp",
                                    args.output / f"{name}-window-b.bmp", orientation)
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
                f"{START_RSVP};{START_AB};15000:ENTER;22000:QUIT", {19000: "abort-results.bmp"}, 35,
                {"CROSSPOINT_SIM_DISPLAY_REFRESH_MS": "1000"},
                language=args.language,
            )
            _read_report(abort_report, complete=False, expected_orientation=ORIENTATION_NAMES[0])

        if args.scenario in ("all", "timeout-recovery"):
            timeout_report, _ = _run(
                program, helper, fixture, args.output.resolve(), "timeout-recovery", 0,
                f"{START_RSVP};{START_AB};85000:QUIT", {80000: "timeout-recovery.bmp"}, 100,
                {"CROSSPOINT_SIM_WINDOW_FAILURE": "busy_timeout_once", "CROSSPOINT_SIM_WINDOW_RECOVER": "1"},
                language=args.language,
            )
            timeout_rows = _read_report(timeout_report, complete=False, expected_orientation=ORIENTATION_NAMES[0])
            if sum(int(row["failure_count"]) for row in timeout_rows) == 0:
                raise RuntimeError("timeout recovery report did not record the failed attempt")

        if args.scenario in ("all", "timeout-deferred"):
            deferred_report, deferred_log = _run(
                program, helper, fixture, args.output.resolve(), "timeout-deferred", 0,
                f"{START_RSVP};{START_AB};78000:BACK;79000:HOME;80000:SLEEP;85000:QUIT",
                {82000: "timeout-deferred.bmp"}, 100,
                {"CROSSPOINT_SIM_WINDOW_FAILURE": "busy_timeout_once"}, language=args.language,
            )
            deferred_rows = _read_report(deferred_report, complete=False,
                                         expected_orientation=ORIENTATION_NAMES[0])
            if sum(int(row["failure_count"]) for row in deferred_rows) == 0:
                raise RuntimeError("deferred-action report did not record the failed attempt")
            forbidden = ("Entering activity: Sleep", "Entering activity: Home", "Exiting activity: RsvpReader")
            if any(marker in deferred_log for marker in forbidden):
                raise RuntimeError("Back, Home, or Sleep escaped while checked display recovery was pending")

        if args.scenario in ("all", "report-failure"):
            _, report_failure_log = _run(
                program, helper, fixture, args.output.resolve(), "report-failure", 0,
                f"{START_RSVP};{START_AB};15000:ENTER;20000:QUIT", {18000: "report-failure.bmp"}, 30,
                block_report=True, language=args.language,
            )
            if "RSVP-SPEED] sample" in report_failure_log:
                raise RuntimeError("window diagnostic emitted hot-path serial samples")

    print(f"RSVP window software validation passed; optical review still required; evidence: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
