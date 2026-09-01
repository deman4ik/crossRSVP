# X3 RSVP Refresh Calibration

Status: instrumentation implemented; physical X3 capture required before enabling playback.

## What the tracer records

Every RSVP presentation logs one bounded sample without allocating a sample list:

```text
RSVP refresh=fast duration=... n=... min=... avg=... max=... buckets=... heap=...
```

The six duration buckets are `≤250`, `≤500`, `≤750`, `≤1000`, `≤2000`, and `>2000` ms. Fast and cleanup samples have independent counters. Normal tracer entry records a fast refresh. A manual full-refresh request records the same paused word through the X3 cleanup (`HALF_REFRESH`) path.
Counters persist across RSVP re-entry for the current boot and reset on reboot, so the sample-count procedure below does not require one long session.

## X3 capture procedure

1. Build and flash the default X3 environment.
2. Open the Russian RSVP fixture or another small Russian EPUB in Paged Mode.
3. Select **RSVP Reading Mode**. Confirm that the first word appears and remains paused.
4. Re-enter the mode until at least 30 fast samples have been collected.
5. Trigger manual refresh at least 10 times to collect cleanup samples.
6. Save the final serial lines for both kinds and record the firmware commit and orientation.

The tracer also logs free heap with every sample. A declining heap trend invalidates the run.

## Playback budget gate

The v0.1 target remains 100 WPM, which gives a 600 ms base interval. Playback must remain disabled until the physical X3 run shows:

- fast-refresh p95 at or below 500 ms, leaving at least 100 ms scheduling margin at 100 WPM;
- free heap above 50 KB with no declining trend;
- cleanup refreshes excluded from the playing deadline and scheduled only at an explicit pause or structural boundary.

The existing X3 HAL documents the cleanup path at approximately 1720 ms, so cleanup cannot be charged to a 600 ms playing frame. If fast p95 exceeds 500 ms, compute the initial safe cap as `floor(60000 / (p95 + 100))` WPM and round down to the nearest 10 WPM. Issue #2 therefore ships with a safe playback budget of **paused only**; issue #3 may enable 100 WPM only after the measurement gate is satisfied.
