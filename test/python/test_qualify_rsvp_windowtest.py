import csv
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "qualify_rsvp_windowtest.py"
SPEC = importlib.util.spec_from_file_location("qualify_rsvp_windowtest", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class WindowQualificationReportTest(unittest.TestCase):
    def write_bmp(self, pixels):
        height = len(pixels)
        width = len(pixels[0])
        row_size = ((width * 3 + 3) // 4) * 4
        pixel_bytes = bytearray()
        for row in reversed(pixels):
            for red, green, blue in row:
                pixel_bytes.extend((blue, green, red))
            pixel_bytes.extend(b"\0" * (row_size - width * 3))
        header = bytearray(54)
        header[:2] = b"BM"
        struct.pack_into("<I", header, 2, len(header) + len(pixel_bytes))
        struct.pack_into("<I", header, 10, len(header))
        struct.pack_into("<IiiHHIIiiII", header, 14, 40, width, height, 1, 24, 0,
                         len(pixel_bytes), 2835, 2835, 0, 0)
        temporary = tempfile.NamedTemporaryFile(delete=False)
        with temporary:
            temporary.write(header)
            temporary.write(pixel_bytes)
        path = Path(temporary.name)
        self.addCleanup(path.unlink)
        return path

    def write_report(self, full_elapsed="60000", window_elapsed="60000", window_count="10",
                     tight_elapsed="60000", tight_count="10", orientation="portrait"):
        fields = [
            "variant", "controller", "confidence", "orientation", "power", "target_wpm", "elapsed_ms",
            "cpu_sample_count", "cpu_min_mhz", "cpu_max_mhz",
            "frame_count", "frame_min_ms", "frame_mean_ms", "frame_max_ms", "interval_count", "interval_min_ms",
            "interval_mean_ms", "interval_max_ms", "refresh_count", "refresh_min_ms", "refresh_mean_ms",
            "refresh_max_ms", "frames_per_minute", "actual_window_count", "actual_full_count",
            "actual_cleanup_count", "fallback_count", "last_fallback", "failure_count",
            "last_operation", "last_successful_operation", "full_warmup_successes", "window_warmup_successes",
            "failure_phase",
            "failure_operation", "failure_requested", "failure_actual", "failure_error", "failure_error_code",
            "failure_duration_ms", "failure_warmup_successes", "failure_last_successful_operation",
            "firmware_version", "diagnostic_stage", "diagnostic_status", "full_probe_confirmed",
            "line_probe_confirmed", "trace_valid", "trace_phase", "trace_stage", "trace_error", "trace_wait",
            "trace_busy_before", "trace_busy_after", "trace_baseline_before", "trace_baseline_after", "trace_x",
            "trace_y", "trace_width", "trace_height", "trace_payload_bytes", "trace_refresh_triggered",
            "tight_probe_confirmed", "roi_sample_count", "roi_min_width", "roi_max_width",
            "roi_min_height", "roi_max_height",
        ]
        temporary = tempfile.NamedTemporaryFile(mode="w", newline="", encoding="ascii", delete=False)
        with temporary:
            writer = csv.DictWriter(temporary, fieldnames=fields)
            writer.writeheader()
            for variant, elapsed, actual_window in (
                ("full", full_elapsed, "0"),
                ("window", window_elapsed, window_count),
                ("tight_window", tight_elapsed, tight_count),
            ):
                row = {field: "0" for field in fields}
                row.update({
                    "variant": variant, "controller": "uc8253", "confidence": "assumed",
                    "orientation": orientation, "power": "not_sampled", "target_wpm": "0",
                    "elapsed_ms": elapsed, "frame_count": "10", "actual_window_count": actual_window,
                    "last_fallback": "none", "last_operation": "full_recovery",
                    "last_successful_operation": "full_recovery", "failure_phase": "none",
                    "failure_operation": "none", "failure_requested": "none", "failure_actual": "none",
                    "failure_error": "none", "failure_last_successful_operation": "none",
                    "firmware_version": "test", "diagnostic_stage": "complete", "diagnostic_status": "complete",
                    "full_probe_confirmed": "1", "line_probe_confirmed": "1", "tight_probe_confirmed": "1",
                })
                if variant == "window":
                    row.update({
                        "roi_sample_count": "10", "roi_min_width": "480", "roi_max_width": "480",
                        "roi_min_height": "32", "roi_max_height": "32",
                    })
                elif variant == "tight_window":
                    row.update({
                        "roi_sample_count": "10", "roi_min_width": "24", "roi_max_width": "32",
                        "roi_min_height": "16", "roi_max_height": "24",
                    })
                writer.writerow(row)
        return Path(temporary.name)

    def test_accepts_complete_sixty_second_pair(self):
        path = self.write_report()
        self.addCleanup(path.unlink)
        rows = MODULE._read_report(path, complete=True, expected_orientation="portrait")
        self.assertEqual([row["variant"] for row in rows], ["full", "window", "tight_window"])

    def test_rejects_short_measurement_or_missing_actual_window(self):
        short = self.write_report(window_elapsed="59999")
        self.addCleanup(short.unlink)
        with self.assertRaisesRegex(RuntimeError, "at least 60 seconds"):
            MODULE._read_report(short, complete=True)
        fallback_only = self.write_report(window_count="0")
        self.addCleanup(fallback_only.unlink)
        with self.assertRaisesRegex(RuntimeError, "actual window"):
            MODULE._read_report(fallback_only, complete=True)
        tight_short = self.write_report(tight_elapsed="59999")
        self.addCleanup(tight_short.unlink)
        with self.assertRaisesRegex(RuntimeError, "at least 60 seconds"):
            MODULE._read_report(tight_short, complete=True)

    def test_rejects_wrong_orientation_metadata(self):
        path = self.write_report(orientation="landscape_cw")
        self.addCleanup(path.unlink)
        with self.assertRaisesRegex(RuntimeError, "does not match requested portrait"):
            MODULE._read_report(path, complete=True, expected_orientation="portrait")

    def test_window_pair_requires_changing_nonblank_line_pixels(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        after = [row[:] for row in before]
        after[3][4] = black
        MODULE._assert_window_pair(self.write_bmp(before), self.write_bmp(after), 0)

        with self.assertRaisesRegex(RuntimeError, "blank"):
            MODULE._assert_window_pair(self.write_bmp([[white] * 8 for _ in range(8)]),
                                       self.write_bmp([[white] * 8 for _ in range(8)]), 0)

    def test_window_pair_rejects_no_update_and_changed_header(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        with self.assertRaisesRegex(RuntimeError, "did not change"):
            MODULE._assert_window_pair(self.write_bmp(before), self.write_bmp(before), 0)

        after = [row[:] for row in before]
        after[3][4] = black
        after[0][0] = black
        with self.assertRaisesRegex(RuntimeError, "outside"):
            MODULE._assert_window_pair(self.write_bmp(before), self.write_bmp(after), 0)

        with self.assertRaisesRegex(RuntimeError, "outside"):
            MODULE._assert_window_pair(self.write_bmp(before), self.write_bmp(after), 1)

    def test_probe_pair_requires_visible_word_transition(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        after = [row[:] for row in before]
        after[3][3] = white
        after[3][4] = black
        MODULE._assert_probe_pair(self.write_bmp(before), self.write_bmp(after))

        with self.assertRaisesRegex(RuntimeError, "did not show"):
            MODULE._assert_probe_pair(self.write_bmp(before), self.write_bmp(before))

    def test_tight_probe_and_frames_are_visible_and_bounded(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        after = [row[:] for row in before]
        after[3][3] = white
        after[3][4] = black
        probe = self.write_bmp(before)
        frame = self.write_bmp(after)
        MODULE._assert_tight_probe_screen(probe)
        report = self.write_report()
        self.addCleanup(report.unlink)
        with report.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        MODULE._assert_tight_pair(probe, frame, rows[2])

    def test_x3_hidpi_roi_mapping_uses_792_by_528_panel_axes(self):
        portrait = {"x": 209, "y": 357, "w": 110, "h": 86}
        landscape = {"x": 338, "y": 222, "w": 110, "h": 86}
        self.assertEqual(
            MODULE._roi_rect_for_screenshot(portrait, 1056, 1584),
            (418, 714, 638, 886),
        )
        self.assertEqual(
            MODULE._roi_rect_for_screenshot(landscape, 1584, 1056),
            (676, 444, 896, 616),
        )

    def test_tight_pair_selection_skips_alias_and_rejects_all_static(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        after = [row[:] for row in before]
        after[3][3] = white
        after[3][4] = black
        first = self.write_bmp(before)
        alias = self.write_bmp(before)
        changed = self.write_bmp(after)
        report = self.write_report()
        self.addCleanup(report.unlink)
        with report.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        requested = (220000, 220137, 220419)
        selected = MODULE._first_distinct_tight_pair(
            [first, alias, changed], requested, rows[2]
        )
        self.assertEqual(selected[:2], (0, 2))

        static = [self.write_bmp(before) for _ in requested]
        with self.assertRaisesRegex(RuntimeError, "no changing screenshot pair"):
            MODULE._first_distinct_tight_pair(static, requested, rows[2])

    def test_roi_trace_proves_tight_rect_is_smaller_and_contains_pixels(self):
        white = (255, 255, 255)
        black = (0, 0, 0)
        before = [[white for _ in range(8)] for _ in range(8)]
        before[3][3] = black
        after = [row[:] for row in before]
        after[3][3] = white
        after[3][4] = black
        probe = self.write_bmp(before)
        frame = self.write_bmp(after)
        report = self.write_report()
        self.addCleanup(report.unlink)
        with report.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        lines = [
            "[100] [INF] [RSVP] diagnostic_roi stage=window_measurement frame=1 "
            "x=0 y=0 w=800 h=64 px=0 py=200 pw=480 ph=32 t=100",
        ] * 10
        lines.append("[105] [INF] [RSVP] group count=1 active=0 words=to||")
        lines.append(
            "[110] [INF] [RSVP] diagnostic_roi stage=tight_probe frame=2 "
            "x=0 y=2 w=8 h=4 px=0 py=220 pw=24 ph=16 t=110"
        )
        for index, (width, height) in enumerate(((24, 16), (32, 24)) * 5, start=3):
            lines.append(
                f"[120] [INF] [RSVP] diagnostic_roi stage=tight_measurement frame={index} "
                f"x=0 y=2 w=8 h=4 px=0 py=220 pw={width} ph={height} t=120"
            )
        _, tight_entries = MODULE._assert_roi_trace("\n".join(lines), probe, rows[2], rows[1])
        MODULE._assert_tight_probe_word("\n".join(lines))
        with self.assertRaisesRegex(RuntimeError, "not 'a'"):
            MODULE._assert_tight_probe_word("\n".join(lines), expected="a")
        MODULE._assert_tight_pair(probe, frame, rows[2], tight_entries)

        broken = [
            line.replace("x=0 y=2 w=8 h=4", "x=0 y=0 w=1 h=1")
            if "stage=tight_measurement" in line else line
            for line in lines
        ]
        with self.assertRaisesRegex(RuntimeError, "outside the logged"):
            _, broken_entries = MODULE._assert_roi_trace("\n".join(broken), probe, rows[2], rows[1])
            MODULE._assert_tight_pair(probe, frame, rows[2], broken_entries)

    def test_tight_metrics_require_confirmation_and_actual_roi(self):
        path = self.write_report()
        self.addCleanup(path.unlink)
        with path.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        MODULE._assert_tight_metrics(rows, complete=True)
        rows[2]["tight_probe_confirmed"] = "0"
        with self.assertRaisesRegex(RuntimeError, "tight_probe_confirmed"):
            MODULE._assert_tight_metrics(rows, complete=True)
        rows[2]["tight_probe_confirmed"] = "1"
        rows[2]["roi_sample_count"] = "0"
        with self.assertRaisesRegex(RuntimeError, "no ROI samples"):
            MODULE._assert_tight_metrics(rows, complete=True)
        rows[2]["roi_sample_count"] = "10"
        rows[1]["roi_sample_count"] = "0"
        with self.assertRaisesRegex(RuntimeError, "window branch has no ROI samples"):
            MODULE._assert_tight_metrics(rows, complete=True)

    def test_busy_not_ready_failure_metadata_survives_recovery(self):
        path = self.write_report(full_elapsed="1000", window_elapsed="1000", window_count="0")
        self.addCleanup(path.unlink)
        rows = []
        with path.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            row.update({
                "failure_count": "0", "last_operation": "full_recovery",
                "last_successful_operation": "full_recovery", "failure_phase": "line_candidate",
                "failure_operation": "line_candidate", "failure_requested": "window",
                "failure_actual": "none", "failure_error": "busy_not_ready", "failure_error_code": "2",
                "failure_duration_ms": "0", "failure_warmup_successes": "0",
                "failure_last_successful_operation": "full_resync",
                "diagnostic_stage": "error", "diagnostic_status": "stopped",
                "full_probe_confirmed": "1", "line_probe_confirmed": "0", "tight_probe_confirmed": "0",
                "trace_valid": "1", "trace_phase": "2", "trace_stage": "2", "trace_error": "2",
                "trace_wait": "0", "trace_busy_before": "0", "trace_busy_after": "0",
                "trace_baseline_before": "2", "trace_baseline_after": "2", "trace_x": "0",
                "trace_y": "200", "trace_width": "480", "trace_height": "32",
                "trace_payload_bytes": "0", "trace_refresh_triggered": "0",
                "roi_sample_count": "0", "roi_min_width": "0", "roi_max_width": "0",
                "roi_min_height": "0", "roi_max_height": "0",
            })
        MODULE._assert_busy_not_ready_failure(rows)
        rows[0]["failure_phase"] = "full_size_ptl"
        with self.assertRaisesRegex(RuntimeError, "failure_phase"):
            MODULE._assert_busy_not_ready_failure(rows)

    def test_busy_timeout_probe_failure_accepts_zero_measurement_failures(self):
        path = self.write_report(full_elapsed="0", window_elapsed="0", window_count="0")
        self.addCleanup(path.unlink)
        with path.open(newline="", encoding="ascii") as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            row.update({
                "failure_phase": "full_size_ptl", "failure_operation": "full_size_ptl",
                "failure_requested": "window", "failure_actual": "none",
                "failure_error": "busy_timeout", "failure_error_code": "3",
                "failure_duration_ms": "0", "failure_warmup_successes": "0",
                "failure_last_successful_operation": "full_resync",
                "last_operation": "full_recovery", "last_successful_operation": "full_recovery",
                "diagnostic_stage": "error", "diagnostic_status": "stopped",
                "full_probe_confirmed": "0", "line_probe_confirmed": "0", "tight_probe_confirmed": "0",
                "trace_valid": "1", "trace_phase": "2", "trace_stage": "2", "trace_error": "3",
                "trace_wait": "0", "trace_busy_before": "0", "trace_busy_after": "0",
                "trace_baseline_before": "2", "trace_baseline_after": "0", "trace_x": "0",
                "trace_y": "0", "trace_width": "792", "trace_height": "528",
                "trace_payload_bytes": "0", "trace_refresh_triggered": "0",
                "roi_sample_count": "0", "roi_min_width": "0", "roi_max_width": "0",
                "roi_min_height": "0", "roi_max_height": "0",
            })
        MODULE._assert_busy_timeout_failure(rows)


if __name__ == "__main__":
    unittest.main()
