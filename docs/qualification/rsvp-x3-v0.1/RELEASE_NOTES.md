# crossRSVP X3 v0.1 (experimental)

This is a **pre-release qualified in host tests and the X3 simulator only**. It has not completed physical X3
endurance, timing, heap, ghosting, battery, install, or rollback acceptance. Use it to perform those checks on an X3;
do not treat it as a stable release.

Scope: Xteink X3 and EPUB. The image uses the shared X3/X4 ESP32-C3 profile, but X4 is not qualified for this release.
Other devices and document formats are out of scope.

Verified before publication:

- 238/238 host tests;
- 9 deterministic X3 simulator scenarios, including Russian RSVP, direct long-Confirm entry, Paged ↔ RSVP position
  transfer, explicit image skipping, chapter boundaries, fatal fallback, and all four orientations;
- PlatformIO cppcheck with no defects;
- production build with no warnings introduced by RSVP;
- embedded version `0.1.0-crossrsvp-experimental-x3`;
- firmware size 5,425,152 bytes;
- SHA-256 `c4d3fe981bb5fb9e5e5bb42e200e536d22bae14ee9ac7d9bae235acae05c20eb`.

Read the [qualification report](README.md), [Russian RSVP button guide](../../rsvp-controls-ru.md), and
[installation, backup, rollback, and X3 test guide](../../rsvp-x3-v0.1-installation.md) before flashing. Record the
physical results in the guide's checklist.
