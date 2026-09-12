# RSVP diagnostic evidence index

This index points to the preserved evidence for the X3 window-update
diagnostics. The directories under `artifacts/` are local, ignored evidence;
they are not ordinary firmware downloads and should not be staged as source
files. The public v0.8.0 release keeps its three model-labelled images and
their checksums separate from these experiments.

## X3 maxspeed diagnostic `0.10.0-windowtest.1`

The diagnostic compares a full-frame branch, a broad RSVP-line branch and a
tight RSVP-line branch. Its ordinary firmware path is unchanged unless the
runtime controller and region pass the safety checks. The latest simulator
evidence is under:

`artifacts/rsvp-x3-maxspeed-validation-v0.10.0/`

The final copied simulator executable is:

`/private/tmp/crossrsvp-simulator-x3-maxspeed-v0.10.0-final-program`

SHA-256: `674e3c1bd12423a07d5f44f4d24c04293aca1b5ec8b9102a0e5eacd2668c380a`.
The simulator matrix covers all four logical orientations and includes the
full, broad-line and tight-line probes, screenshot checks, failure/recovery,
abort and report cases. Simulator results do not qualify a physical panel.

The diagnostic firmware package is preserved separately:

`artifacts/crossrsvp-x3-v0.10.0-windowtest.1/`

The image
`crossrsvp-x3-v0.10.0-windowtest.1.bin` has SHA-256
`dca44a4b0f5212e3d27cacc6841ae8f5958d8b54fa51a502546dbb4b9f1181df`.
The four-entry package ZIP
`artifacts/crossrsvp-x3-v0.10.0-windowtest.1.zip` has SHA-256
`fe325e93d61dde742f68b57895c283e79bfc23fd11a5fdc0ef4172ae149af6ee`.

## Latest physical CSV

The user-provided X3 result is copied byte-for-byte to:

`artifacts/rsvp-x3-maxspeed-validation-v0.10.0/hardware-result-2026-09-12-user-01/rsvp-window-test.csv`

SHA-256: `866f24d9b91ab72a4bc58af994e8942563545f89b93d366654b2c1fdf8063c86`.
It identifies firmware `crossRSVP-v0.10.0-windowtest.1-x3`, controller
`UC8253` with `assumed` confidence, and portrait orientation. All three rows
completed without failures or fallbacks and all three probe-confirmation flags
are set.

| Variant | Elapsed | Frames | Whole-time rate | Mean refresh | Actual window / full | ROI samples |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `full` | 60,163 ms | 114 | 113.691139/min | 489 ms | 0 / 114 | 0 |
| `window` | 60,056 ms | 130 | 129.878780/min | 425 ms | 129 / 1 | 129 |
| `tight_window` | 60,115 ms | 130 | 129.751310/min | 417 ms | 129 / 1 | 129 |

The broad branch is +14.2383% versus full in this run. Tight is +14.1261%
versus full and −0.0981% versus broad, so it added no measured throughput over
the broad branch. The user reported no traces of previous words; that remains a
visual observation rather than CSV telemetry. This is one physical unit and
one orientation, with no X4 or X4 Pro hardware evidence and no claim about an
absolute electrical panel maximum.

The earlier `0.9.1-windowtest.1` full/window CSV remains at its original path:

`artifacts/rsvp-x3-windowtest-validation-v0.9.1/hardware-result-2026-09-12-user-01/rsvp-window-test.csv`

The historical `0.9.0` stripe report remains at:

`artifacts/rsvp-x3-windowtest-validation/hardware-2026-09-12/rsvp-window-test.csv`

Both are retained as scoped history and must not be mixed with the current
three-branch run when calculating a release result.

## Release boundary

The diagnostic package is a test instrument for X3. It is not the ordinary
CrossRSVP v0.8.0 image and does not qualify X4 or X4 Pro. See
[the v0.8.0 release notes](rsvp-release-notes.md) and
[the device audit](rsvp-device-audit.md) for the per-model release status.
