# CrossRSVP v0.7.0 guide

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

The pace starts at 60 WPM, uses 10 WPM steps, and defaults to 100 WPM.
The provisional ceiling is 130 WPM on X3/UC8253, 120 WPM on X4 or X4 Pro/SSD1677,
and 100 WPM on uncalibrated controller variants. See the
[display timing evidence](rsvp-display-limits.md). A slow e-ink refresh limits
the observed rate; RSVP does not skip groups to catch up. Long Back follows
the configured home/file-browser action.

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
operation of those paths remains part of the pending v0.6.0 audit.

## Grouping language for the open book

Open **Grouping language** in the book's reading menu. On X4 Pro it is also
available through the paused RSVP panel's **Settings → Reader** page. Choose
**Auto**, **Russian**, or **English**; the choice is saved independently for each
book and survives rebuilding derived caches. The global short-word grouping
switch must still be enabled.

Auto uses the first EPUB language element, independently of menu language.
Supported English/Russian tag variants select the corresponding policy; missing,
malformed, or unsupported primary languages leave words standalone. Later language
elements do not replace an empty/unsupported first value. Manual choices apply to
the entire book.

One companion is limited to seven letters; two companions to five letters each.
The active word keeps its existing fit rules. English adds 48 exact forward forms
and words with an internal straight/curly apostrophe: `John's BOOK`, `I don't KNOW`.
The Russian inventory is unchanged. Rejected candidates remain in source order.
Changing language while paused retains the current group and acknowledged history;
future groups use the new choice.

## Release artifacts and qualification

Use only the image matching the reader model. Firmware paths are `artifacts/crossrsvp-<device>-v0.7.0/firmware`; simulator
evidence is in `artifacts/issues-13-14`. The consolidated firmware
packages are local build outputs; see [v0.7.0 validation](rsvp-v0.7.0-validation.md). Compare each image with
its generated `SHA256SUMS` file. Host and simulator results cannot establish
panel, touch, battery, SDMMC, USB MSC, sleep, or heap behavior on hardware.
