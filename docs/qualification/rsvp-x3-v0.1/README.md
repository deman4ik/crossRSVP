# crossRSVP X3 v0.1 qualification

Status: **simulator-qualified release candidate; physical X3 acceptance pending**.

This report qualifies the experimental RSVP implementation with host tests, deterministic X3 simulator scenarios,
static analysis, and a production firmware build. It does not promote the candidate to hardware-qualified status.
Only Xteink X3 and EPUB are in the qualification scope. The firmware uses the shared X3/X4 ESP32-C3 board profile,
but it is not qualified or released for X4. X4 Pro, X4C, Sticky, TXT, PDF, Markdown, XTC, and other devices or formats
are also outside this candidate's scope.

## Candidate

- PlatformIO environment: `rsvp_x3_release`
- Embedded version: `0.1.0-crossrsvp-experimental-x3`
- Firmware: `artifacts/rsvp-x3-v0.1/firmware/crossrsvp-x3-v0.1-experimental.bin` (local, ignored build artifact)
- Published pre-release: [crossRSVP X3 v0.1 (experimental)](https://github.com/deman4ik/crossRSVP/releases/tag/v0.1.0-crossrsvp-x3)
- Size: 5,424,960 bytes
- SHA-256: `9f9b31de00d43a3b69e00aa1f5325b1e251dc364b945837f695584406eb42279`
- Static RAM estimate: 56,260 / 327,680 bytes (17.2%)
- Flash image usage: 5,411,253 / 6,553,600 bytes (82.6%)

Reproduce and package the build with:

```bash
pio run -e rsvp_x3_release -j 1
```

The post-build step copies `.pio/build/rsvp_x3_release/firmware.bin` to the candidate path above and writes
`artifacts/rsvp-x3-v0.1/firmware/SHA256SUMS`. The manual
`Build experimental crossRSVP X3 candidate` GitHub Actions workflow runs the same environment and uploads the image,
checksum, ELF, map, and partitions files for diagnostics. The permanent pre-release asset is the exact locally
qualified binary identified by the hash above. Each independently compiled Actions artifact must be identified by its
own checksum.

The build completed without warnings originating in RSVP sources. Remaining warnings came from framework and external
WebSocket/RainMaker code.

## Automated and simulator results

| Gate | Result |
| --- | --- |
| Host GoogleTest suite | 236/236 passed |
| Repository C/C++ formatting wrapper | Passed |
| PlatformIO static analysis, low through high | Passed, no defects |
| Production X3 build | Passed |
| Deterministic X3 simulator matrix | Passed |

Run the simulator matrix with:

```bash
pio run -e simulator_x3
python3 scripts/qualify_rsvp_simulator.py
```

The script creates an isolated synthetic Russian EPUB and settings tree. It does not touch the developer's real
`fs_` books or settings. The simulator dependency is pinned in `platformio.ini` so the same device model is used on
repeated runs.

| Scenario | Evidence | Result |
| --- | --- | --- |
| RSVP opens paused with Russian text and ORP guides | [Paused](screenshots/flow-portrait-paused.png) | Passed |
| Active playback advances in source order | [Playing](screenshots/flow-portrait-playing.png) | Passed |
| Back returns to Paged Mode and highlights the last word | [Paged highlight](screenshots/flow-portrait-paged-highlight.png) | Passed |
| Re-entry repeats the last RSVP word | [RSVP again](screenshots/flow-portrait-rsvp-again.png) | Passed |
| Unsupported image pauses with a Paged Mode prompt | [Image boundary](screenshots/boundary-image.png) | Passed |
| New chapter pauses before its first word | [Chapter boundary](screenshots/boundary-chapter.png) | Passed |
| Injected source-open failure falls back to Paged Mode | [Fatal fallback](screenshots/fatal-fallback-paged.png) | Passed |
| All four X3 orientations render a paused RSVP frame | [0](screenshots/orientation-0-paused.png), [1](screenshots/orientation-1-paused.png), [2](screenshots/orientation-2-paused.png), [3](screenshots/orientation-3-paused.png) | Passed |

The fatal scenario uses the simulator-only `CROSSPOINT_SIM_RSVP_FATAL_LOAD` fault injection seam. It is excluded from
production builds.

## User-story coverage

| Stories from issue #1 | Current evidence | Physical X3 follow-up |
| --- | --- | --- |
| 1–12: entry, exclusivity, controls, pace | Host controller/activity tests and paused/playing/mode-switch simulator flow | Verify every mapped button and the 60–120 WPM selector |
| 13–14: refresh-aware pacing and safe cap | Fake-clock host tests prove deadline accounting and no token skipping | Measure real frame intervals; ordinary intervals must stay within 10% |
| 15–27: ORP, Russian Unicode, punctuation, hyphens, fitting | Host Unicode/rendering tests plus Russian simulator frames | Inspect `ё`, `й`, dialogue punctuation, smallest font, and unrenderable long-word prompt |
| 28–34: page/chapter/content boundaries and mode position | Host mode/source tests plus chapter, image, highlight, and repeat simulator evidence | Exercise a page turn between modes and confirm the displayed-page restart rule |
| 35–39: checkpoints, revision, recovery, progress, statistics | Host storage/session tests cover every checkpoint trigger, corruption, revision invalidation, and progress/statistics decisions | Verify sleep/resume, SD writes, native progress, and statistics on device |
| 40–43: localization, orientations, cleanup, fatal fallback | Host localization/fallback tests and simulator orientation/fatal scenarios | Assess panel ghosting, cleanup cadence, low-memory behavior, and readable error rendering |
| 44: diagnostics | Bounded timing, heap, checkpoint, and word-order logging is implemented | Capture real serial logs for the 30-minute run |
| 45: safe installation and rollback | [Installation and rollback guide](../../rsvp-x3-v0.1-installation.md) prepared from the implemented update paths | Perform and record backup, install, rollback, and cache-reset drills |

## Required physical X3 acceptance

Do not describe this candidate as hardware-qualified until all items below are recorded against the exact firmware
hash above:

- Back up the SD card and full 16 MiB flash, then verify both backup hashes.
- Install the candidate using the documented SD or custom web-flasher path and confirm the embedded version.
- Read a Russian EPUB for 30 minutes at 100 WPM with serial logging enabled. Record start/end battery percentage.
- Confirm no missing or reordered words, crash, reset, or memory exhaustion.
- Sample ordinary word intervals throughout the run; each must remain within 10% of the requested pace outside explicit
  punctuation, paragraph, boundary, and cleanup pauses.
- Record fast-refresh and cleanup distributions. Free heap must remain above 50 KB with no declining trend.
- Confirm ghosting stays readable and cleanup never consumes or skips a word.
- Verify menu entry, long-Confirm mapping, play/pause, rewind five, step one, pace changes, Back, long-Back, and mutual
  exclusion with Paged Mode.
- Verify mode switching both without and after a Paged page turn, including highlight, repeat, and displayed-page restart.
- Verify checkpoint saves at 30 seconds, pause, mode switch, chapter transition, sleep, and exit.
- Verify sleep/resume, changed Book Revision, corrupted checkpoint, native progress, and reading-time statistics.
- Verify chapter, image/table, oversized word, fatal recovery, and all four orientations.
- Perform the rollback and cache-reset drills, then confirm an EPUB opens in Paged Mode.

Use the record sheet in the [installation guide](../../rsvp-x3-v0.1-installation.md#physical-x3-record-sheet) and attach
the completed results before calling the release qualified.

## Simulator limitations

Simulator refresh durations are host-computer timings, not E-Ink timings. The qualification script supplies a fixed
65,536-byte free-heap value and 32,768-byte maximum-allocation value only to exercise the firmware's safety threshold;
those values are not device measurements. The simulator cannot establish physical WPM accuracy, panel ghosting,
battery behavior, deep-sleep recovery, SD reliability, USB flashing, or boot-recovery behavior.
