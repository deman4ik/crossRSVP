# CrossRSVP v0.9.0 validation

This release contains three separate production images built from the same
source commit and the pinned FreeInk SDK revision. The release manifest records
the commit, SDK revision, exact embedded version marker, image size and SHA-256
for each file. The release is not a diagnostic build.

| Model | Production configuration | Prior physical evidence |
| --- | --- | --- |
| X3 | Established wide-line update with full-buffer controller write; short RAM write disabled | One portrait X3: 130 frames in 60.056 s (129.88 frames/min), no old-word traces reported |
| X4 | Established wide-line update with full-buffer controller write; short RAM write disabled | No physical measurement yet; this is the path from the preceding production release |
| X4 Pro | 20 MHz SPI and wide-line-only short RAM write for UC8279; unsupported controllers retain the full-frame fallback | One portrait UC8279 X4 Pro: 122 frames/min, mean refresh 469 ms, no display errors or old-word traces reported |

The X3 short RAM experiments are excluded: despite clean words and page
recovery, they ended in a display error. The X4 Pro narrow word ROI is also
excluded because previous characters remained visible. Hardware observations
above cover only the named devices and orientation. They do not establish a
maximum electrical panel speed, long-term ghosting behavior, or qualification
of other controller variants.

Local release checks:

- `pio run -e rsvp_x3_release`, `pio run -e rsvp_x4_release`, and
  `pio run -e rsvp_x4pro_release`: completed successfully with no compiler
  warnings in the final logs.
- `pio check -e rsvp_x4pro_release`: passed with no defects.
- `npm test` in `site/`: 12 tests passed.
- `npm run test:browser` in `site/`: passed in Chromium, covering both languages,
  release cards, RSVP demo, keyboard access and mobile gutters.
- `git diff --check` and the repository's `./bin/clang-format-fix -g` passed.
- Each packaged image's SHA-256 and embedded model/version marker were checked
  before upload; the SHA-256 list is included as `SHA256SUMS`.

PlatformIO reported 56,888 bytes RAM and 5,520,813 bytes flash for each C3
target (X3 and X4), and 100,496 bytes RAM and 5,420,066 bytes flash for X4 Pro.
All three fit their configured limits of 327,680 bytes RAM and 6,553,600 bytes
application flash. These are build-size reports, not free heap measurements
during reading.

The simulator cannot exercise the E-Ink controller's physical BUSY timing or
the UC8279 RAM write. A device check remains necessary for X4, other controller
variants and the three orientations outside the observed portrait runs.
