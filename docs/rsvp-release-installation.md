# CrossRSVP v0.8.0 — X3, X4 and X4 Pro installation and rollback

This guide applies to the model-labelled images in the CrossRSVP v0.8.0 GitHub
pre-release. The ordinary X3, X4 and X4 Pro images have completed their local
build and package checks, but GitHub marks the release **beta / pre-release**
while physical qualification is pending. Use only the image whose model name
matches the label on the reader.

| Reader | Image | Controller behavior in RSVP |
| --- | --- | --- |
| Xteink X3 | `crossrsvp-x3-v0.8.0.bin` | UC8253 can use the RSVP line update; an inconclusive X3 probe or unsupported active driver uses a full frame |
| Original Xteink X4 | `crossrsvp-x4-v0.8.0.bin` | SSD1677 and UC8279 line paths are available; an unsupported active driver uses the full frame |
| Xteink X4 Pro | `crossrsvp-x4pro-v0.8.0.bin` | SSD1677 and UC8279 line paths are available; UC8179 Pro remains on the full-frame fallback |

The image label selects the board profile. The active driver and the model must
remain compatible; this is not a reason to install an image built for another
model. The line update may reduce the refreshed area, but actual pace and image
quality depend on the panel, LUT and BUSY timing. It is not a promise of the
same gain for every word group or every device batch.

Before updating, copy books and settings from the SD card. Keep the previous
CrossRSVP image and its SHA-256 beside the backup. The checked-in updater reads
`.bin` files from the SD card, validates the image header, size, chip, board
tag, checksum, and SHA-256, then writes the inactive OTA partition before
switching to it. Do not remove power during that operation.

1. Confirm the exact model and compare the downloaded image's SHA-256 with the
   `SHA256SUMS` entry from the same GitHub release.
2. Copy the matching image from the release bundle to the SD card:
   `crossrsvp-x3-v0.8.0.bin`, `crossrsvp-x4-v0.8.0.bin`, or
   `crossrsvp-x4pro-v0.8.0.bin`. Do not select a neighbouring model's image.
3. On the reader open **Settings → System → SD Card Firmware Update**, select
   the image, wait for validation, and confirm the update.
4. Leave the reader powered until it reports completion and restarts. The
   updater revalidates the removable file immediately before writing.
5. Open an EPUB and perform the device checklist in
   [the device audit](rsvp-device-audit.md). RSVP starts from the existing
   reader position and is entered explicitly on a paused screen.

The current build evidence, CI status and physical checks are listed in the
[v0.8.0 validation report](rsvp-v0.8.0-validation.md). The local image checks
do not set `hardware_qualified`.

The X4 Pro's USB MSC transfer mode temporarily owns the SD card. Finish copying
files, then use the computer's safe-eject/unmount command and wait for the host
to release the drive. Only after that, exit USB Transfer on the reader and wait
for its reboot before unplugging the cable or editing the card again. The C3
release profiles do not enable USB MSC.

## Try RSVP after installation

Open an EPUB and choose **More → RSVP Reading Mode** in the toolbar, or choose
**RSVP Reading Mode** in the list menu. Reading starts paused. Confirm the
highlighted letter and the words from the book before continuing. If the
controller supports the line path, only the RSVP line may refresh; a full-frame
refresh is expected for a fallback controller or an uncertain probe.

During the first check, compare a short Russian and English passage at the same
requested pace. Look for readable line edges, contrast, repeated words and
ghosting. Record the model, controller shown by the diagnostic/status report,
orientation and image SHA-256 before reporting a problem. Physical evidence for
one X3 portrait board does not qualify X4, X4 Pro, or the other orientations.

## Rollback

Rollback uses the same SD-card update flow: verify the saved image's SHA-256,
select it by its exact model label, and wait for a complete reboot. Restore the
backed-up SD contents after the rollback image has booted.

If the normal UI is unavailable, the firmware has a model-specific recovery
hatch. On X3 and original X4, hold logical **Up** while pressing Power to wake;
on X4 Pro, hold logical **Down** while pressing Power. The firmware logs the
recovery mode and opens the same SD firmware picker. The Pro's Down key is GPIO7;
it is used because its Up key is the ESP32-S3 strap GPIO0. Recovery is still
subject to the firmware reaching its input initialization; it is not a ROM or
bootloader rescue path. If that hatch cannot be reached, stop rather than
trying an image for another model and use the project's supported service
recovery procedure.

The release artifact directory is generated after a successful build. It is
the source of truth for the image filename and checksum. The website links to
the exact model asset from GitHub; it does not substitute a local diagnostic
image.

For a reproducible qualification build, use the repository's generated local
PlatformIO override with `extra_configs` blank (for example,
`platformio.qualification.local.ini`) so a developer's ignored
`platformio.local.ini` cannot silently change the dependency revision or build
flags. Keep that override outside Git and record its effective dependency
revision with the build report.
