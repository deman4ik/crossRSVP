# crossRSVP X3 v0.4.0 qualification

Status: **host- and simulator-qualified experimental candidate; physical X3 qualification pending**.

Implements [issue #11](https://github.com/deman4ik/crossRSVP/issues/11) and
[Presentation Groups](../../specs/0005-rsvp-presentation-groups.md). Context Line is removed, grouping defaults off,
and Focus Reading is restored directly under Reader while retaining Text Settings > Style.

## Candidate identity

- Embedded version: `0.4.0-crossrsvp-experimental-x3`
- Firmware: `artifacts/rsvp-x3-v0.4.0/firmware/crossrsvp-x3-v0.4.0-experimental.bin`
- Size: 5,435,584 bytes.
- SHA-256: `e4c2b2f45b0f6f1fd8baa4bc11678410aa97ab1758d39305bb471aa316ac7735`.
- Only the dedicated CrossRSVP X3/simulator version is incremented; the upstream standard-target version is unchanged.

## Automated checks

- `cmake --build build/test -j 4` and `ctest --test-dir build/test --output-on-failure`: 294/294 passed,
  including 120 RSVP tests. Coverage includes the 50+8 exact forms, boundaries, prefix/postfix overflow, whole-token
  width rejection, fixed ORP, additive timing, acknowledgement, rewind, checkpoint identity and migration.
- `pio run -e default`: passed; 5,466,163 / 6,553,600 bytes application flash and
  56,276 / 327,680 bytes static RAM. These are link-time sizes, not available runtime heap.
- `pio run -e simulator_x3`: passed for version 0.4.0.
- Release qualification uses a separate `PLATFORMIO_BUILD_DIR`: a concurrent PlatformIO run with another
  configuration invalidated the shared `.pio/build` during the first attempt. No source error caused that failure.
- `rsvp_x3_release` passed: 5,421,869 / 6,553,600 bytes application flash, 56,260 / 327,680 bytes static RAM,
  and 123,173 / 321,296 bytes detailed DRAM occupancy. The WebSockets deprecated `flush()` warning is pre-existing.
- `scripts/qualify_rsvp_simulator.py`: all 15 scenarios passed; evidence under
  `artifacts/rsvp-x3-v0.4.0/simulator/`.
  Frame diagnostics verify `Я` → `не сделал бы` → `это,` → `а он пришёл` → `в дом.` in source order;
  a three-word frame is captured in all four orientations. Paged re-entry resumes at `это,`, not the saved `бы`.
  Existing single-word flow, boundaries, quick entry, fatal fallback, controls during a 450 ms refresh and durable
  Paged highlight restoration are included. Focus Reading was enabled in Reader, observed enabled in Style,
  disabled in Style, and observed disabled again in Reader; the final persisted setting was also checked.
- Formatting uses `./bin/clang-format-fix -g`; `git diff --check` passes. Deleted tests were staged before formatting
  because the existing wrapper includes deleted tracked files; new tests were marked intent-to-add.

## Fixed storage and font ownership

The session retains four `DocumentEvent` slots: three displayed words and one lookahead. The immutable group view
points into those slots. Six history descriptors replace the old word history; each is bounded by a compile-time
24-byte assertion (at most 144 bytes total). Rewind reopens the source at the first token and restores recorded
membership without calling the width resolver again. Classification, group construction and layout add no heap
containers or per-frame text copies.

The Russian policy tables are `constexpr` data. ESP32-C3 object inspection places the core and experimental tables
in `.rodata` (400 and 64 bytes respectively, excluding the string literals). SD-font grouping reuses activity-owned advance scratch and reserves
advance tables at entry. A smaller available SD font is loaded once through the existing font manager, which owns
and reuses it until family unload; this cannot be stack storage because its glyph/advance data must survive frames.
If setup allocation fails, grouping falls back to single-word rendering. No second framebuffer is introduced.

Simulator heap values are injected constants, **not memory measurements**. No claim of hardware heap stability,
timing, visual comfort or flash-install safety follows from simulator success. SD-font cache behavior and non-Russian
shaping still require device checks, as does repeated switching between settings and playback.

## Physical acceptance still required

Use the [installation/rollback guide](../../rsvp-x3-v0.4.0-installation.md), preserving the previous firmware and SD data.
On the exact candidate checksum:

- compare grouping off/on at the same WPM in representative Russian prose; subjective fixation comfort is sufficient;
- check all orientations, minimum/larger built-in sizes, installed SD font sizes, long words and whole-word overflow;
- check pause, forward, rewind, Paged switch, sleep/restart and explicit Paged navigation;
- find Focus Reading in Reader and Text Settings > Style and verify changes in either path are reflected in the other;
- monitor free heap, largest free block, stack high-water mark, refresh timing and ghosting over at least 30 minutes;
- verify installation and rollback on X3 before promoting the experimental candidate.

Physical checks have not been performed in this implementation session. Firmware and simulator artifacts are ignored
build products and are not committed.
