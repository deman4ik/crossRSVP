#!/usr/bin/env python3
"""Qualify RSVP-SPEED timing in the X3 diagnostic simulator.

This harness expects an already-built simulator executable.  It deliberately
does not invoke PlatformIO: pass the executable with ``--program``.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
FIXTURE_BUILDER = REPO_ROOT / "scripts" / "build_rsvp_speed_fixture.py"
SIM_HELPER = REPO_ROOT / "scripts" / "qualify_rsvp_simulator.py"
ORIENTATIONS = range(4)
INJECTED_REFRESH_MS = 300


def _load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load simulator helper: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def input_script() -> str:
    events = [(1200, "ENTER:500"), (2800, "LEFT"), (3500, "LEFT"),
              (4200, "LEFT"), (4900, "ENTER"), (7800, "ENTER")]
    events.extend((9000 + index * 600, "RIGHT") for index in range(23))
    events.extend(((23400, "ENTER"), (33000, "ENTER"),
                   (35400, "ENTER"), (41400, "QUIT")))
    return ";".join(f"{when}:{event}" for when, event in events)


def speed_samples(log: str) -> list[dict[str, int | str]]:
    samples = []
    for line in log.splitlines():
        if "[RSVP-SPEED] sample " not in line:
            continue
        values = dict(re.findall(r"([A-Za-z_]+)=([^\s,]+)", line))
        required = ("run", "frame", "pace", "kind", "refresh_ms", "frame_ms", "interval_ms", "words", "heap")
        if not all(field in values for field in required):
            continue
        try:
            sample = {field: int(values[field]) for field in required if field != "kind"}
            sample["kind"] = values["kind"]
        except ValueError:
            continue
        samples.append(sample)
    return samples


def validate(log: str, settings_path: Path, orientation: int, screenshot: Path) -> None:
    samples = speed_samples(log)
    if not samples:
        raise RuntimeError(f"orientation-{orientation}: no RSVP-SPEED samples")
    if any(sample["refresh_ms"] < INJECTED_REFRESH_MS for sample in samples):
        raise RuntimeError(f"orientation-{orientation}: refresh_ms below injected {INJECTED_REFRESH_MS}ms")
    if {int(s["pace"]) for s in samples} != {50, 600}:
        raise RuntimeError(f"orientation-{orientation}: expected samples at 50 and 600 words/min")
    seen: dict[int, set[int]] = {}
    for sample in samples:
        frames = seen.setdefault(int(sample["run"]), set())
        frame = int(sample["frame"])
        if frame in frames:
            raise RuntimeError(f"orientation-{orientation}: duplicate frame {frame} in run {sample['run']}")
        if not frames and sample["interval_ms"] != 0:
            raise RuntimeError("new run must establish a zero-interval baseline")
        if frames and sample["interval_ms"] < INJECTED_REFRESH_MS:
            raise RuntimeError("non-baseline interval is shorter than refresh")
        frames.add(frame)
    runs = sorted({int(sample["run"]) for sample in samples})
    if len(runs) < 3 or not any(int(sample["interval_ms"]) == 0 for sample in samples[1:]):
        raise RuntimeError(f"orientation-{orientation}: pause/resume did not produce a zero-interval new run")
    if not any(int(sample["interval_ms"]) >= INJECTED_REFRESH_MS for sample in samples):
        raise RuntimeError(f"orientation-{orientation}: no raw interval_ms >= {INJECTED_REFRESH_MS}ms")
    settings = json.loads(settings_path.read_text(encoding="utf-8"))
    if settings.get("rsvpPaceWpm") != 100:
        raise RuntimeError(f"orientation-{orientation}: rsvpPaceWpm={settings.get('rsvpPaceWpm')!r}, expected unchanged 100")
    if not screenshot.is_file() or screenshot.stat().st_size < 64:
        raise RuntimeError(f"orientation-{orientation}: missing screenshot {screenshot}")


def run_orientation(program: Path, output: Path, orientation: int, helper, fixture: Path) -> None:
    name = f"orientation-{orientation}"
    log_path = output / f"{name}.log"
    screenshot = output / f"{name}.bmp"
    with tempfile.TemporaryDirectory(prefix=f"crossrsvp-speed-{orientation}-") as temp:
        run_root = Path(temp)
        helper.write_simulator_state(run_root, fixture, orientation, True, 14, 0, 100, False, False, "RU")
        env = os.environ.copy()
        env["CROSSPOINT_SIM_INPUT_SCRIPT"] = helper.delayed_inputs(input_script())
        env["CROSSPOINT_SIM_SCREENSHOTS"] = helper.screenshot_schedule(output, {8500: f"{name}-minimum.bmp", 32000: f"{name}-playing.bmp", 34000: screenshot.name})
        env["CROSSPOINT_SIM_FREE_HEAP"] = "65536"
        env["CROSSPOINT_SIM_MAX_ALLOC_HEAP"] = "32768"
        env["CROSSPOINT_SIM_DISPLAY_REFRESH_MS"] = str(INJECTED_REFRESH_MS)
        completed = subprocess.run([str(program)], cwd=run_root, env=env, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60, check=False)
        log_path.write_text(completed.stdout, encoding="utf-8")
        if completed.returncode:
            raise RuntimeError(f"{name}: simulator exited {completed.returncode}; see {log_path}")
        validate(completed.stdout, run_root / "fs_" / ".crosspoint" / "settings.json", orientation, screenshot)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path, help="built simulator executable")
    parser.add_argument("--output", required=True, type=Path, help="directory for logs and four screenshots")
    args = parser.parse_args(argv)
    program = args.program.resolve()
    if not program.is_file():
        raise SystemExit(f"simulator program not found: {program}; build it separately before running this harness")
    args.output.mkdir(parents=True, exist_ok=True)
    helper = _load_module(SIM_HELPER, "qualify_rsvp_simulator_speed_helper")
    with tempfile.TemporaryDirectory(prefix="crossrsvp-speed-fixture-") as temp:
        fixture = Path(temp) / "rsvp-speed-fixture.epub"
        subprocess.run([sys.executable, str(FIXTURE_BUILDER), str(fixture), "--language", "ru", "--words", "720"],
                       cwd=REPO_ROOT, check=True)
        for orientation in ORIENTATIONS:
            print(f"[simulator] speed orientation-{orientation}", flush=True)
            run_orientation(program, args.output.resolve(), orientation, helper, fixture)
    print(f"RSVP speed qualification passed; evidence: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
