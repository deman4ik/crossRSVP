# CrossRSVP v0.8.0 — validation report

This report records the verification evidence for the ordinary CrossRSVP
v0.8.0 firmware work for Xteink X3, X4 and X4 Pro. GitHub publication remains
a pre-release / beta step while physical model checks are completed. The final
normal v0.8.0 images are recorded as built and packaged below. The report does
not set `hardware_qualified`: simulator and host results do not qualify a
physical panel.

## Automated evidence

The following checks are recorded as passing:

- Native RSVP tests: **167/167 PASS**. The source suite is under `test/rsvp/`.
- `DisplayRegion`: **9/9 PASS**. This source was unchanged after the prior
  geometry validation.
- Python window qualification tests: **13/13 PASS** from
  `test/python/test_qualify_rsvp_windowtest.py`.
- FreeInk SDK host checks: `run_pro.py`, `run_uc8253_power.py`,
  `run_uc8279.py`, and the real-bus `run_epd_bus.py` all pass. The pinned SDK
  revision is `fde240faaeae6c340dacd435a4f77d2ef2f82dfd`; the tracked
  integration patch is
  `scripts/freeink_patches/x3-rsvp-window.patch` with SHA-256
  `9c80b5030e5d6b99747e1c9cb1e6e8542ea8190344ad3c7befe30f56f84f4a17`.
  The checked BUSY tests cover no-start, delayed-start, stuck-active, bounded
  completion, and suppression of writes after a checked failure.
- Site generator checks: **12/12 PASS**, and the static site build passes.
  Chromium browser checks were unavailable in this environment and remain
  unverified.
- The pinned `rsvp_x3_release` PlatformIO static check passed with **0
  defects** in **1m51.137s**. This is a static check; it does not claim that
  physical behavior is qualified; final image and package evidence is recorded
  separately below.

No CI result is claimed by these results; CI and publication checks remain
separate from the completed local image builds.

## X3 simulator evidence

The evidence is preserved under
`artifacts/crossrsvp-v0.8.0-validation/simulator-x3/`.

The four orientation logs each show `production_gate used=1`, a successful
full-frame baseline, and subsequent `normal_window` ROI updates:

| Orientation | Logged physical ROI |
| ---: | --- |
| 0 | `x=344 y=3 w=112 h=522` |
| 1 | `x=8 y=209 w=784 h=109` |
| 2 | `x=344 y=3 w=112 h=522` |
| 3 | `x=8 y=210 w=784 h=109` |

`production-window-unsupported.log` records the unsupported/inverted fallback
with `production_gate used=0` and no `production_roi` entries. The line path is
therefore not treated as available in that case. The recovery log records a
checked display failure (`error=3`), a recovery full frame, a later successful
window ROI, and the Back action. `flow-portrait.log` records paused entry,
play/pause, paged fallback and RSVP re-entry.

The simulator logs identify their embedded program as
`crossRSVP-v0.7.0-simulator`, so they are behavior evidence for the simulator
flow rather than evidence from a final v0.8.0 firmware image. The preserved
hash record distinguishes the binary used for QA from the rebuilt binary after
temporary simulator-only logging was removed:

```text
fa07f42b39134ea87598aa0005e1c744a3abc3868b3d8bb40db8a11b79cdeba0  prior simulator_x3 program used for matrix/recovery QA
ce7e6a309436737f642e60d373459b96f029edc4a96a011ba7ab731050db8355  final simulator_x3 program after simulator-only production_gate log removal
```

The artifact note records the two simulator programs as behavior-equivalent
for this logging-only rebuild. It does not turn either program into the final
normal v0.8.0 firmware or qualify a physical X3 panel.

## X4 simulator evidence

The X4 simulator build and matrix artifacts are preserved under
`artifacts/crossrsvp-v0.8.0-validation/simulator-x4/`. The build took
**4m03.863s**; `program.sha256` records:

```text
c613bae25d2c7ee7ecd1034ef7c6718273b969ff9cdcfce0d84b7202de965084  .pio/build/simulator_x4/program
```

The four normal-window orientation runs, the inverted/unsupported fallback,
and the checked-failure recovery case passed their simulator assertions. The
normal-window logs report these physical ROI bounds:

| Orientation | Logged physical ROI |
| ---: | --- |
| 0 | `x=344 y=3 w=120 h=474` |
| 1 | `x=8 y=185 w=792 h=109` |
| 2 | `x=344 y=3 w=120 h=474` |
| 3 | `x=8 y=186 w=792 h=109` |

These logs also identify the embedded simulator program as
`crossRSVP-v0.7.0-simulator`; they are simulator-flow evidence and do not
qualify a physical X4 panel or replace the final normal v0.8.0 image/package
record below.

## X4 Pro simulator build evidence

The X4 Pro simulator program is preserved under
`artifacts/crossrsvp-v0.8.0-validation/simulator-x4pro/program`. The initial
build took **5m45.765s**; a necessary rebuild took **19.56s** and produced the
same copied-program SHA-256:

```text
6b40f0949d35274f970fc63f300c3d2be31d01d2ca2b2749859c2f3da9dadedc  artifacts/crossrsvp-v0.8.0-validation/simulator-x4pro/program
```

The X4 Pro simulator QA matrix **PASS** is preserved under the same artifact
directory. All four production orientations passed their assertions against
the archived logs and screenshots; touch controls and orientation 0 were
verified with the same copied program, while orientations 1–3 passed in the
final run. The Pro inverted-orientation cases used a harness-only raw-direction
input correction; firmware sources and the copied program were unchanged. The
shared unsupported/recovery behavior is covered by the X3/X4 matrices above
and was not duplicated for Pro. This remains simulator evidence only: it does
not qualify a physical X4 Pro panel or the physical behavior of the final
normal v0.8.0 firmware package.

## Ordinary X4 release image

The ordinary `rsvp_x4_release` build completed successfully in **4m59.831s**
and embeds `crossRSVP-v0.8.0-x4`. The reported resource usage is **56,888 /
327,680 bytes RAM (17.4%)** and **5,519,443 / 6,553,600 bytes flash
(84.2%)**. The packaged binary is **5,533,520 bytes** at
`artifacts/crossrsvp-v0.8.0-release/firmware/crossrsvp-x4-v0.8.0.bin`; its
SHA-256 is:

```text
24cbf7acbdf0f31916bf125ba89387abe910ca35dc030ea1dbbb7e974404b81e
```

The corresponding build metadata is preserved under
`artifacts/crossrsvp-v0.8.0-release/build/x4/`. This verifies the X4 image
and package metadata; it does not qualify a physical X4 panel.

## Ordinary X4 Pro release image

The ordinary `rsvp_x4pro_release` build completed successfully in **4m34.822s**
with no reported warnings or errors and embeds `crossRSVP-v0.8.0-x4pro`. The
reported resource usage is **100,496 / 327,680 bytes RAM (30.7%)** and
**5,418,370 / 6,553,600 bytes flash (82.7%)**. The optional link-time regions
reported **169,230 / 341,760 bytes DIRAM (49.52%)** and **16,384 / 16,384
bytes IRAM (100%)**; those region figures are not free-heap measurements. The
packaged binary is **5,423,392 bytes** at
`artifacts/crossrsvp-v0.8.0-release/firmware/crossrsvp-x4pro-v0.8.0.bin`; its
SHA-256 is:

```text
76108752044478586bb3b33b4dc8b09cff28a7b8738067fd7fe42f80795d35b2
```

The corresponding build metadata is preserved under
`artifacts/crossrsvp-v0.8.0-release/build/x4pro/`. This verifies the X4 Pro
image and package metadata; it does not qualify a physical X4 Pro panel.

## Ordinary X3 release image

The ordinary `rsvp_x3_release` build completed successfully in **5m44.509s**
and embeds `crossRSVP-v0.8.0-x3`. The reported resource usage is **56,888 /
327,680 bytes RAM (17.4%)** and **5,519,443 / 6,553,600 bytes flash
(84.2%)**. The packaged binary is **5,533,520 bytes** at
`artifacts/crossrsvp-v0.8.0-release/firmware/crossrsvp-x3-v0.8.0.bin`; its
SHA-256 is:

```text
b164c7c07a14bc6cf3991c94327115b204c080c4ce47d8ca0077d400002b99ad
```

The build metadata under `artifacts/crossrsvp-v0.8.0-release/build/x3/` was
checked against the package hook. This verifies the X3 image and package
metadata; it does not qualify a physical X3 panel.

## Physical X3 observation from the separate diagnostic

The only physical observation recorded here comes from the diagnostic
`0.10.0-windowtest.1` run, not from the normal v0.8.0 image. It was one X3 in
portrait orientation, with `uc8253` and `assumed` controller confidence,
`target_wpm=0` (uncapped diagnostic pacing), and no reported failures or
fallbacks. The user observed no traces of previous words; that observation is
not telemetry.

Raw CSV:
`artifacts/rsvp-x3-maxspeed-validation-v0.10.0/hardware-result-2026-09-12-user-01/rsvp-window-test.csv`.

| Variant | Elapsed / frames | Whole-run rate* | Frame ms min/mean/max | Interval ms min/mean/max | Refresh ms min/mean/max | Actual full/window | ROI samples / geometry |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `full` | 60,163 ms / 114 | 113.69/min | 500/522/1,594 | 503/518/536 | 479/489/1,561 | 114 / 0 | 0 / 0×0 |
| `window` | 60,056 ms / 130 | 129.88/min | 437/458/1,595 | 438/453/470 | 416/425/1,562 | 1 / 129 | 129 / 120×522 |
| `tight_window` | 60,115 ms / 130 | 129.75/min | 438/459/1,602 | 439/453/470 | 408/417/1,560 | 1 / 129 | 129 / 96×67–128 |

\* Whole-run rate is `frame_count × 60,000 ÷ elapsed_ms`, so the denominator
is the complete branch duration. In this one run, `window` was 14.24% faster
than `full` by that measure; `tight_window` was 14.13% faster than `full` and
0.10% below `window`. These values describe one physical unit and one
orientation. They do not qualify the normal v0.8.0 image, other orientations,
X4, X4 Pro, another panel batch, or an absolute electrical maximum.

## Remaining release checks

| Check | Status at this report checkpoint |
| --- | --- |
| Pinned `rsvp_x3_release` static check | **PASS** — 0 defects, 1m51.137s |
| Final normal v0.8.0 X3 firmware build, package and embedded-version check | **PASS** — 5m44.509s, embedded `crossRSVP-v0.8.0-x3`, package/hash above |
| X4 simulator build and four-orientation normal/recovery/fallback matrix | **PASS** — simulator artifacts and final image/package are recorded above |
| Final normal v0.8.0 X4 firmware image, package and embedded-version check | **PASS** — 4m59.831s, embedded `crossRSVP-v0.8.0-x4`, package/hash above |
| X4 Pro simulator build and four-orientation production QA | **PASS** — archived logs/screens above; harness raw-direction correction only |
| Final normal v0.8.0 X4 Pro firmware image, package and embedded-version check | **PASS** — 4m34.822s, embedded `crossRSVP-v0.8.0-x4pro`, package/hash above |
| Release package SHA-256 records | **PASS** — all three model image hashes are recorded above |
| CI workflow and publication checks | **PENDING** — not run in this local validation checkpoint |
| Chromium browser checks | **UNVERIFIED** — browser environment unavailable |
| Physical normal v0.8.0 installation and panel qualification | **PENDING** |

The future device record must cover installation, RSVP controls, line edges and
ghosting in all four orientations for each model, with the exact model image
and controller result recorded. A serial trace is not required for that
checklist. The X3 diagnostic observation above does not qualify the ordinary
firmware as a physical model check. The final normal images now exist, but the
model records and physical panel checks are still pending, so
`hardware_qualified` remains `false`.

## Reproduction references

The focused host checks can be rerun with:

```sh
python3 freeink-sdk/libs/display/FreeInkDisplay/test/host/run_pro.py
python3 freeink-sdk/libs/display/FreeInkDisplay/test/host/run_uc8253_power.py
python3 freeink-sdk/libs/display/FreeInkDisplay/test/host/run_uc8279.py
python3 freeink-sdk/libs/display/FreeInkDisplay/test/host/run_epd_bus.py
python3 -m unittest test/python/test_qualify_rsvp_windowtest.py
```

The simulator and physical diagnostic artifacts are retained separately so a
diagnostic build is not confused with the ordinary release image.

## Related documents

- [Release notes](rsvp-release-notes.md)
- [Installation and rollback](rsvp-release-installation.md)
- [Device audit](rsvp-device-audit.md)
- [Diagnostic evidence index](rsvp-diagnostics-index.md)
