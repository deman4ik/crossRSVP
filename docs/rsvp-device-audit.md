# CrossRSVP v0.8.0 device audit

This audit records the device facts used by the v0.8.0 release profiles. It
separates SDK support from CrossRSVP behavior and from physical qualification.
The release remains a GitHub pre-release / beta until each image has been
exercised on the named board.

| Area | Xteink X3 | Original Xteink X4 | Xteink X4 Pro | CrossRSVP release status |
| --- | --- | --- | --- | --- |
| MCU and memory | ESP32-C3, no PSRAM | ESP32-C3, no PSRAM | ESP32-S3, 8 MB PSRAM | X3/X4 use the tuned C3 core; Pro uses the prebuilt S3 `dio_opi` core |
| Display | 792×528; UC8253, with newer UC8279d variant | 800×480; SSD1677, UC8179, or UC8279 drivers linked | 800×480; SSD1677, UC8179, or UC8279 drivers linked | Driver paths: UC8253 on X3, SSD1677/UC8279 on X4 and X4 Pro; unsupported active drivers use full frame, and UC8179 Pro remains full-frame fallback |
| Input | Physical button path | Physical button path | GT911 touch, capacitive Home, Left/Right and Power buttons | Logical C3 controls are retained; Pro RSVP Control Panel is implemented, with 13 simulator touch/gesture scenarios passed, including four orientations; physical touch qualification pending |
| Frontlight | None | None | Warm/cool adjustable light | Pro capability is compiled through the SDK profile; C3 UI must not promise it |
| RTC / IMU | RTC and QMI8658 IMU | No RTC/IMU in the profile | RTC, no IMU | Existing device capability gates apply; tilt remains X3-only |
| Storage | SD over SPI | SD over SPI | Native 1-bit SDMMC | Pro release enables `USE_BLOCK_DEVICE_INTERFACE=1`; all access remains through HAL |
| USB transfer | No USB MSC in this release profile | No USB MSC in this release profile | USB MSC capability in SDK profile | Pro release enables USB MSC using the prebuilt TinyUSB graph; C3 USB behavior is outside this release promise |
| Sleep / resume | Shared CrossPoint lifecycle | Shared CrossPoint lifecycle | Shared CrossPoint lifecycle | RSVP position preservation must be checked on hardware |

The current source distinguishes compiled support from a particular board's
measured hardware. `BoardConfig.h:103-140` links SSD1677 for the X4 family and
links UC8179/UC8279 X4 drivers; `BoardConfig.h:394-400` defines the controller
variants and the live VER/FLG probe. `FreeInkDisplay.cpp:143-167` selects the
UC8179, UC8279, or SSD1677 driver from the active controller. The X3 probe
selects the UC8253 or UC8279d sibling in `XteinkDetect.cpp:563-573`.

## RSVP line behavior

RSVP keeps the highlighted reference letter in one place while words from the
book change. When a checked controller and region allow it, CrossRSVP updates
the RSVP line rather than the whole page. The full-frame path remains available
for an unsupported, uncertain or not-ready controller. A smaller update region
can reduce refresh work on a particular panel, but the firmware does not promise
the same acceleration for every group, panel batch or controller state.

Inverted display colors and night mode use a full-frame refresh. The line path
is not claimed for those modes.

The v0.8.0 mapping used for release review is:

- X3: UC8253 line path; an inconclusive X3 probe uses a full-frame fallback.
- Original X4: SSD1677 and UC8279 line paths; an unsupported active driver uses
  a full frame.
- X4 Pro: SSD1677 and UC8279 line paths; UC8179 stays on the full-frame fallback.

The model image and the runtime controller result must both be recorded in a
qualification report. A controller driver being linked in the SDK proves source
support; it does not prove that a panel revision has acceptable contrast,
ghosting, BUSY timing or refresh speed.

The MCU, touch, storage, and capability claims come from the checked-in SDK
profiles and support notes:

- `freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h`
- `freeink-sdk/docs/xteink-x3-uc8279-support.md`
- `freeink-sdk/docs/xteink-x4pro-support.md`
- `docs/specs/0006-device-adaptation-and-release.md`

No Bluetooth feature is inferred from these profiles. Bluetooth-related SDK
code is outside the CrossRSVP release promise.

## Qualification checklist

The build itself proves the profile and embedded version; it does not prove
panel refresh quality, input wiring, battery behavior, or recovery. The latest
physical observation is one X3 in portrait orientation: the full branch
accepted 114 frames in 60,163 ms, the broad line branch accepted 130 frames in
60,056 ms, and the tight line branch accepted 130 frames in 60,115 ms. The
controller was UC8253 with `assumed` confidence, and the run reported no
failures or fallbacks. The raw result is indexed in
[the diagnostic evidence index](rsvp-diagnostics-index.md). It does not qualify
the X3 model as a whole, either other orientation, X4, or X4 Pro.

For each physical model record the board revision, image SHA-256, boot result,
EPUB open, RSVP entry, pause/resume, pace change, Back, sleep/wake, SD access,
and USB transfer where supported. Exercise all four logical orientations on
every model, including X4 Pro. Fill measured heap and refresh observations after
the build and bench run; do not turn an unmeasured result into a release claim.
The release manifest therefore keeps `hardware_qualified: false` until those
records exist for each image.

Until those fields are recorded, the release should remain a pre-release even
though its public title can use the non-experimental CrossRSVP name required by
the specification.
