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
                     orientation="portrait"):
        fields = [
            "variant", "controller", "confidence", "orientation", "power", "target_wpm", "elapsed_ms",
            "cpu_sample_count", "cpu_min_mhz", "cpu_max_mhz",
            "frame_count", "frame_min_ms", "frame_mean_ms", "frame_max_ms", "interval_count", "interval_min_ms",
            "interval_mean_ms", "interval_max_ms", "refresh_count", "refresh_min_ms", "refresh_mean_ms",
            "refresh_max_ms", "frames_per_minute", "actual_window_count", "actual_full_count",
            "actual_cleanup_count", "fallback_count", "last_fallback", "failure_count",
        ]
        temporary = tempfile.NamedTemporaryFile(mode="w", newline="", encoding="ascii", delete=False)
        with temporary:
            writer = csv.DictWriter(temporary, fieldnames=fields)
            writer.writeheader()
            for variant, elapsed, actual_window in (
                ("full", full_elapsed, "0"), ("window", window_elapsed, window_count)
            ):
                row = {field: "0" for field in fields}
                row.update({
                    "variant": variant, "controller": "uc8253", "confidence": "assumed",
                    "orientation": orientation, "power": "not_sampled", "target_wpm": "300",
                    "elapsed_ms": elapsed, "frame_count": "10", "actual_window_count": actual_window,
                    "last_fallback": "none",
                })
                writer.writerow(row)
        return Path(temporary.name)

    def test_accepts_complete_sixty_second_pair(self):
        path = self.write_report()
        self.addCleanup(path.unlink)
        rows = MODULE._read_report(path, complete=True, expected_orientation="portrait")
        self.assertEqual([row["variant"] for row in rows], ["full", "window"])

    def test_rejects_short_measurement_or_missing_actual_window(self):
        short = self.write_report(window_elapsed="59999")
        self.addCleanup(short.unlink)
        with self.assertRaisesRegex(RuntimeError, "at least 60 seconds"):
            MODULE._read_report(short, complete=True)
        fallback_only = self.write_report(window_count="0")
        self.addCleanup(fallback_only.unlink)
        with self.assertRaisesRegex(RuntimeError, "actual window"):
            MODULE._read_report(fallback_only, complete=True)

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


if __name__ == "__main__":
    unittest.main()
