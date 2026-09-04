# crossRSVP X3 v0.3.0 qualification

Status: **host- and simulator-qualified; physical X3 qualification pending**.

## Candidate identity

- Embedded version: `0.3.0-crossrsvp-experimental-x3`
- Firmware: `artifacts/rsvp-x3-v0.3.0/firmware/crossrsvp-x3-v0.3.0-experimental.bin`
- Size: 5,434,768 bytes
- SHA-256: `6db0df2fddc59c8e21d53faa49ed5376fdc22b0e0b748219795a910c670cc21a`

## Automated evidence

- Host suite: 280/280 tests passed. Context Line coverage includes default-off persistence, English and Russian UI,
  real Russian EPUB token order, fixed ORP layout, whole-token omission, bounded nearest-first context, deferred source
  errors, stable pause, step/rewind, durable checkpoints and all content boundaries.
- X3 simulator: all scenarios passed in headless SDL. Context Line was exercised during continuous playback with a
  stable simulated heap, as well as in all four orientations. Existing Paged ↔ RSVP, image skip, quick entry, chapter
  boundary, fatal fallback, high-speed controls under injected 450 ms refresh latency and highlight restore scenarios
  also passed.
- Release build: `rsvp_x3_release` passed in 8m 5s. PlatformIO reports 5,421,057 / 6,553,600 bytes application
  flash (82.7%), 56,260 / 327,680 bytes static RAM (17.2%), and 123,173 / 321,296 bytes detailed DRAM
  occupancy (38.34%). The only emitted compiler warnings were pre-existing third-party/API and incomplete-enum switch
  warnings outside the RSVP change.

Simulator evidence is generated under `artifacts/rsvp-x3-v0.3.0/simulator/`; build products are intentionally not
committed.

## Required physical X3 checks

Before promoting the candidate, follow the [installation and rollback guide](../../rsvp-x3-v0.3.0-installation.md)
and record results against the exact SHA-256 from the final build:

- enable Context Line and verify its readability, fixed ORP and full-token omission at every orientation;
- confirm context does not cross sentence, paragraph, chapter, image or other non-text boundaries;
- pause, step and rewind, confirming the context changes around only the Active Word;
- leave RSVP during an active refresh and confirm the last physically presented word is highlighted;
- sleep/restart, reopen the EPUB and confirm the saved Active Word is highlighted in Paged Mode;
- re-enter RSVP without navigation and confirm the same word is shown paused with empty left context;
- perform explicit page/chapter/TOC/percentage/bookmark/link navigation and confirm the old highlight/checkpoint is
  superseded;
- run the existing 30-minute Russian EPUB, timing, controls, heap, ghosting, battery, cache and rollback checks.

Until those checks pass, the release remains experimental and X3-only.
