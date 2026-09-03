# crossRSVP X3 v0.2 qualification

Status: **host- and simulator-qualified; physical X3 qualification pending**.

## Candidate identity

- Embedded version: `0.2.0-crossrsvp-experimental-x3`
- Firmware: `artifacts/rsvp-x3-v0.2/firmware/crossrsvp-x3-v0.2-experimental.bin`
- Size: 5,427,376 bytes
- SHA-256: `98968d2af08fd6a2507e5ae5d938b4c521149047574fde2e2f8211d5a9d997da`

## Automated evidence

- Host suite: 256/256 tests passed, including Russian token identity, acknowledged-frame checkpointing, failed
  checkpoint invalidation and retry, explicit Paged navigation, cache preservation, EPUB deletion, and mode routing.
- X3 simulator: all scenarios passed in headless SDL — Paged ↔ RSVP flow, exact-word re-entry, image skip, quick entry,
  chapter boundary, fatal fallback, high-speed controls under injected 450 ms refresh latency, and all four
  orientations.
- Release build: `rsvp_x3_release` succeeded in 14:31.09.
- Linker summary: 5,413,665 / 6,553,600 bytes application flash (82.6%); 56,260 / 327,680 bytes statically reported
  RAM (17.2%). Detailed ESP-IDF section accounting reports 123,173 / 321,296 bytes DRAM (38.34%).
- The only compiler warning is the existing third-party WebSockets `NetworkClient::flush()` deprecation.

The simulator evidence is generated under `artifacts/rsvp-x3-v0.2/simulator/`; build products are intentionally not
committed.

## Required physical X3 checks

Before promoting the candidate, follow the [installation and rollback guide](../../rsvp-x3-v0.2-installation.md) and
record results against the exact SHA-256 above:

- install and rollback from SD;
- 30-minute Russian EPUB session at 100 WPM;
- requested 120/180/240 WPM timing and refresh distribution;
- Pause, Back, pace and navigation commands pressed during active refresh;
- exact-word persistence across RSVP → Paged → RSVP and reboot;
- explicit page/chapter/TOC/percentage/bookmark/link navigation superseding the old RSVP position;
- derived-cache clear preserving progress and checkpoint, while EPUB deletion removes both;
- end-of-book native progress retention and checkpoint removal;
- heap trend, watchdog/reset log, ghosting and battery delta;
- all four orientations and non-text boundary behavior.

Until those checks pass, the release remains experimental and X3-only.
