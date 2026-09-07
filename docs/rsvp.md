# CrossRSVP v0.5.0 guide

CrossRSVP is an explicit EPUB reading mode. A book opens in Paged Mode first;
RSVP starts paused at the current reading position and shares its checkpoint
with Paged Mode. Images, tables, and other unsupported content pause RSVP and
offer the existing paged or skip actions.

## X3 and original X4

Use the logical buttons so the guide follows the selected button mapping:

| Action | Command |
| --- | --- |
| Start or pause | Confirm |
| Return to Paged Mode | Back |
| Step while paused | Next Page |
| Rewind five presentation groups | Previous Page |
| Decrease/increase pace | Left / Right |

The pace range is 60–240 WPM in 10 WPM steps, with 100 WPM as the default
request. A slow e-ink refresh limits the observed rate; RSVP does not skip
groups to catch up. Long Back follows the configured home/file-browser action.

## X4 Pro

The X4 Pro keeps the same RSVP command semantics and adds a touch route. Enter
RSVP by tapping the centre of the page. In the default toolbar choose **More →
RSVP Reading Mode**; in the classic list menu choose **RSVP Reading Mode** directly. Playback hides
the panel. Tapping the reading area while playing pauses and opens the RSVP
Control Panel; the opening tap cannot activate a control. On the paused panel,
use explicit controls for Continue, one Step, Rewind five groups, pace down/up,
return to Paged Mode, and applicable settings. Step, rewind, and pace changes
remain paused. Closing the panel does not resume playback.

System Home, Back, power, sleep, and lighting gestures retain their existing
priority. The panel follows the active orientation and preserves the shared
reading position. The X4 Pro release profile has GT911 touch, capacitive Home,
warm/cool frontlight, PSRAM, native SDMMC, and USB MSC support; physical
operation of those paths remains part of the pending v0.5.0 audit.

## Release artifacts and qualification

Use only the image matching the reader model. Firmware and simulator
paths are `artifacts/crossrsvp-<device>-v0.5.0/{firmware,simulator}`; the
consolidated bundle is `artifacts/crossrsvp-v0.5.0`. Compare each image with
its generated `SHA256SUMS` file. Host and simulator results cannot establish
panel, touch, battery, SDMMC, USB MSC, sleep, or heap behavior on hardware.
