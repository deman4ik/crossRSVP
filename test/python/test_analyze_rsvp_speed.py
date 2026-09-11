import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).parents[2] / "scripts" / "analyze_rsvp_speed.py"
SPEC = importlib.util.spec_from_file_location("analyze_rsvp_speed", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def row(run, frame, pace, kind, refresh, interval, words):
    return f"[RSVP-SPEED] sample run={run} frame={frame} pace={pace} kind={kind} refresh_ms={refresh} frame_ms=100 interval_ms={interval} words={words} heap=90000"


class AnalyzeRsvpSpeedTest(unittest.TestCase):
    def test_mix_cleanup_and_first_sample_excluded(self):
        text = "\n".join((row(1, 1, 600, "fast", 100, 0, 1), row(1, 2, 600, "fast", 110, 100, 2), row(1, 3, 600, "cleanup", 300, 200, 3), row(1, 4, 600, "fast", 120, 100, 4)))
        results = MODULE.analyse(MODULE.parse_serial(text))
        fast = next(item for item in results if item["kind"] == "fast")
        cleanup = next(item for item in results if item["kind"] == "cleanup")
        self.assertEqual(fast["count"], 2)
        self.assertEqual(fast["measured_cadence"], 600)
        self.assertEqual(fast["word_throughput"], 1800)
        self.assertEqual(cleanup["count"], 1)

    def test_csv_has_same_parser(self):
        csv_text = "run,frame,pace,kind,refresh_ms,frame_ms,interval_ms,words,heap\n2,1,600,fast,90,100,0,1,80000\n2,2,600,fast,90,100,100,1,80000\n"
        self.assertEqual(len(MODULE.parse_csv(csv_text)), 2)

    def test_mixed_total_and_invalid_or_empty_data(self):
        text = "\n".join((row(3, 1, 600, "fast", 100, 0, 1), row(3, 2, 600, "fast", 100, 300, 1), row(3, 3, 600, "cleanup", 500, 1700, 1), row(3, 4, 600, "fast", 100, 300, 1), "[RSVP-SPEED] sample run=3 frame=x pace=600 kind=fast refresh_ms=bad frame_ms=1 interval_ms=1 words=1 heap=1"))
        results = MODULE.analyse(MODULE.parse_serial(text))
        total = next(item for item in results if item["kind"] == "all")
        self.assertEqual(total["count"], 3)
        self.assertEqual(total["interval_ms_min"], 300)
        self.assertEqual(total["interval_ms_max"], 1700)
        self.assertAlmostEqual(total["measured_cadence"], 60000 * 3 / 2300)
        self.assertEqual(MODULE.parse_serial(""), [])
        self.assertEqual(MODULE.parse_serial("[RSVP-SPEED] sample run=1 frame=1 pace=600 kind=fast refresh_ms=-1 frame_ms=1 interval_ms=-1 words=1 heap=1"), [])

    def test_config_starts_new_session(self):
        text = row(1, 1, 600, "fast", 100, 0, 1) + "\n[RSVP-SPEED] config model=x controller=y cap=600 version=v\n" + row(1, 1, 600, "fast", 100, 0, 1)
        samples = MODULE.parse_serial(text)
        self.assertEqual([sample["session"] for sample in samples], [1, 2])


if __name__ == "__main__":
    unittest.main()
