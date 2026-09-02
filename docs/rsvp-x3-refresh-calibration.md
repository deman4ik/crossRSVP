# X3 RSVP Refresh Calibration

Status: instrumentation implemented and playback enabled for the simulator-qualified experimental candidate; physical
X3 capture is still required before hardware qualification. See the
[v0.1 qualification report](qualification/rsvp-x3-v0.1/README.md).

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

The default and qualification baseline remain 100 WPM, which gives a 600 ms base interval. The experimental control
range extends to 240 requested WPM (250 ms base interval) so a faster X3 panel can be exercised. Blocking refresh time
is charged to every frame: when it exceeds the requested interval, the next word appears as soon as the panel is ready,
without skipping tokens. A requested value above 100 must not be described as an achieved physical rate until measured.

The candidate must not be described as physically qualified until the X3 run shows:

- fast-refresh p95 at or below 500 ms, leaving at least 100 ms scheduling margin at 100 WPM;
- free heap above 50 KB with no declining trend;
- cleanup refreshes excluded from the playing deadline and scheduled only at an explicit pause or structural boundary.

The existing X3 HAL documents the cleanup path at approximately 1720 ms, so cleanup cannot meet an ordinary playing
interval. Compute the qualified safe pace as `floor(60000 / (p95 + 100))` WPM and round down to the nearest 10 WPM;
the extra 100 ms retains the existing scheduling margin. Keep 100 WPM as the qualification baseline unless the
physical run proves a higher rate; the 240-WPM control ceiling is an experimental request, not a hardware claim.
