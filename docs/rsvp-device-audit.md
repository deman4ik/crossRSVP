# CrossRSVP device audit

This audit records the device facts used by the release profiles. It separates
SDK support from CrossRSVP behavior and from physical qualification. The last
column remains pending until each image has been exercised on the named board.

| Area | Xteink X3 | Original Xteink X4 | Xteink X4 Pro | CrossRSVP release status |
| --- | --- | --- | --- | --- |
| MCU and memory | ESP32-C3, no PSRAM | ESP32-C3, no PSRAM | ESP32-S3, 8 MB PSRAM | X3/X4 use the tuned C3 core; Pro uses the prebuilt S3 `dio_opi` core |
| Display | 792×528; UC8253, with newer UC8279d variant | 800×480; SSD1677, UC8179, or UC8279 drivers linked | 800×480; SSD1677, UC8179, or UC8279 drivers linked | Current source links the family drivers and probes the live controller; refresh, panel revision, and ghosting remain local qualification checks |
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
panel refresh quality, input wiring, battery behavior, or recovery. For each
physical model record the board revision, image SHA-256, boot result, EPUB open,
RSVP entry, pause/resume, pace change, Back, sleep/wake, SD access, and USB
transfer where supported. Exercise all four logical orientations on every
model, including X4 Pro. Fill measured heap and refresh observations after the
build and bench run; do not turn an unmeasured result into a release claim.

Until those fields are recorded, the release should remain a pre-release even
though its public title can use the non-experimental CrossRSVP name required by
the specification.
