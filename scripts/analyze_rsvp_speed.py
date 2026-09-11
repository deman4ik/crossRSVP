#!/usr/bin/env python3
"""Summarise RSVP-SPEED serial samples.

The input may be a normal serial log containing ``[RSVP-SPEED]`` records or a
CSV file whose headers are the sample fields.  Configuration records are
accepted and ignored for the numeric summary.
"""

import argparse
import csv
import io
import re
import sys
from collections import defaultdict
from pathlib import Path

FIELDS = ("run", "frame", "pace", "kind", "refresh_ms", "frame_ms", "interval_ms", "words", "heap")
_RECORD = re.compile(r"\[RSVP-SPEED\]\s+(?P<body>.*)")
_PAIR = re.compile(r"(?P<key>[A-Za-z_]+)=(?P<value>[^\s,]+)")
_INT_FIELDS = {field for field in FIELDS if field != "kind"}


def _sample(values):
    if any(field not in values for field in FIELDS):
        return None
    try:
        result = {field: values[field] for field in FIELDS}
        for field in _INT_FIELDS:
            result[field] = int(result[field])
            if result[field] < 0:
                return None
        if result["kind"] not in ("fast", "cleanup"):
            return None
        return result
    except (TypeError, ValueError):
        return None


def parse_serial(text):
    """Return sample dictionaries parsed from serial text."""
    samples = []
    session = 1
    for line in text.splitlines():
        if "[RSVP-SPEED]" in line and " config " in line:
            session += 1
            continue
        match = _RECORD.search(line)
        if match:
            sample = _sample({m.group("key"): m.group("value") for m in _PAIR.finditer(match.group("body"))})
            if sample is not None:
                sample["session"] = session
                samples.append(sample)
    return samples


def parse_csv(text):
    """Return sample dictionaries from a CSV with the documented headers."""
    samples = []
    for row in csv.DictReader(io.StringIO(text)):
        sample = _sample(row)
        if sample is not None:
            samples.append(sample)
    return samples


def parse_input(text, csv_mode=False):
    if csv_mode:
        return parse_csv(text)
    # A header is an unambiguous indication of CSV, while serial logs may also
    # contain commas in unrelated diagnostic lines.
    first = next((line for line in text.splitlines() if line.strip()), "")
    return parse_csv(text) if "run,frame,pace" in first else parse_serial(text)


def _percentile(values, percentile):
    ordered = sorted(values)
    if not ordered:
        return 0
    position = (len(ordered) - 1) * percentile / 100
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    value = ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)
    return int(value) if value.is_integer() else round(value, 2)


def analyse(samples):
    """Group samples and calculate measured cadence and word throughput."""
    groups = defaultdict(list)
    for sample in samples:
        groups[(sample.get("session", 1), sample["run"], sample["pace"])].append(sample)
    results = []
    for (session, run, pace), all_rows in sorted(groups.items()):
        for kind, rows in (("all", all_rows), ("fast", [r for r in all_rows if r["kind"] == "fast"]), ("cleanup", [r for r in all_rows if r["kind"] == "cleanup"])):
            intervals = [row["interval_ms"] for row in rows if row["interval_ms"] > 0]
            elapsed = sum(intervals)
            refresh = [row["refresh_ms"] for row in rows]
            frame = [row["frame_ms"] for row in rows]
            result = {"session": session, "run": run, "pace": pace, "kind": kind, "count": len(intervals),
                      "measured_cadence": (60000 * len(intervals) / elapsed) if elapsed else 0,
                      "word_throughput": (60000 * sum(row["words"] for row in rows if row["interval_ms"] > 0) / elapsed) if elapsed else 0}
            for name, values in (("refresh_ms", refresh), ("frame_ms", frame), ("interval_ms", intervals)):
                result.update({f"{name}_{stat}": _percentile(values, pct) for stat, pct in (("min", 0), ("median", 50), ("p95", 95), ("max", 100))})
            result["technical_refresh_bound_frames_per_min"] = (60000 / _percentile(refresh, 95)) if kind == "fast" and refresh and _percentile(refresh, 95) else 0
            results.append(result)
    return results


def _number(value):
    return str(int(value)) if isinstance(value, (int, float)) and value == int(value) else f"{value:.2f}"


def format_report(results):
    lines = []
    for result in results:
        keys = ("session", "run", "pace", "kind", "count", "refresh_ms_min", "refresh_ms_median", "refresh_ms_p95", "refresh_ms_max", "frame_ms_min", "frame_ms_median", "frame_ms_p95", "frame_ms_max", "interval_ms_min", "interval_ms_median", "interval_ms_p95", "interval_ms_max", "measured_cadence", "word_throughput", "technical_refresh_bound_frames_per_min")
        line = " ".join(f"{key}={result[key] if key == 'kind' else _number(result[key])}" for key in keys)
        if result["kind"] == "fast":
            line += " technical_refresh_bound_note=display_only_no_overhead_no_safe_max_guarantee"
        lines.append(line)
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="serial.log or RSVP-SPEED CSV")
    parser.add_argument("--csv", action="store_true", help="force CSV input")
    args = parser.parse_args(argv)
    results = analyse(parse_input(args.input.read_text(encoding="utf-8", errors="replace"), args.csv))
    if results:
        print(format_report(results))
    else:
        print("No RSVP-SPEED samples found", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
