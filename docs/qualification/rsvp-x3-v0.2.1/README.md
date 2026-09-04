# crossRSVP X3 v0.2.1 qualification

Status: **host- and simulator-qualified; physical X3 qualification pending**.

## Candidate identity

- Embedded version: `0.2.1-crossrsvp-experimental-x3`
- Firmware: `artifacts/rsvp-x3-v0.2.1/firmware/crossrsvp-x3-v0.2.1-experimental.bin`
- Size: 5,427,792 bytes
- SHA-256: `ea71c15011a2a5dfbf361864a68ad99bf22236e49611a9c604ae3cced867dfcb`

## Automated evidence

- Host suite: 257/257 tests passed, including checkpoint presence and recovery, Russian token identity, explicit Paged
  navigation, cache preservation and mode routing.
- X3 simulator: all scenarios passed in headless SDL. The new `highlight-reopen` scenario first reproduced the defect,
  then verified pixel-identical Active Word highlighting immediately after RSVP exit and after a fresh sleep/wake
  process reopens the book in Paged Mode.
- Existing scenarios also passed: Paged ↔ RSVP flow, image skip, quick entry, chapter boundary, fatal fallback,
  high-speed controls under injected 450 ms refresh latency and all four orientations.
- Release build: `rsvp_x3_release` succeeded in 12:34.72.
- Linker summary: 5,414,075 / 6,553,600 bytes application flash (82.6%); 56,260 / 327,680 bytes statically reported
  RAM (17.2%). Detailed ESP-IDF section accounting reports 123,173 / 321,296 bytes DRAM (38.34%).
- The only compiler warning is the existing third-party WebSockets `NetworkClient::flush()` deprecation.

Simulator evidence is generated under `artifacts/rsvp-x3-v0.2.1/simulator/`; build products are intentionally not
committed.

## Required physical X3 checks

Before promoting the candidate, follow the [installation and rollback guide](../../rsvp-x3-v0.2.1-installation.md)
and record results against the exact SHA-256 above:

- leave RSVP during an active refresh and confirm the last physically presented word is highlighted;
- sleep/restart, reopen the EPUB and confirm the saved Active Word is highlighted in Paged Mode;
- re-enter RSVP without navigation and confirm the same word is shown paused;
- perform explicit page/chapter/TOC/percentage/bookmark/link navigation and confirm the old highlight/checkpoint is
  superseded;
- run the existing 30-minute Russian EPUB, timing, controls, heap, ghosting, battery, orientation, cache and rollback
  checks.

Until those checks pass, the release remains experimental and X3-only.
