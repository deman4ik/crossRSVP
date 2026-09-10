# CrossRSVP v0.6.0 — X3, X4 и X4 Pro: installation and rollback

This guide applies to the model-labelled images in the CrossRSVP v0.6.0 release.
Physical qualification is pending; this is a pre-release. Use
only the image whose model name matches the label on the reader. X3 and the
original X4 are separate package names even though they share the ESP32-C3
build and runtime detection. X4 Pro requires the ESP32-S3 image.

Before updating, copy books and settings from the SD card. Keep the previous
CrossRSVP image and its SHA-256 beside the backup. The checked-in updater reads
`.bin` files from the SD card, validates the image header, size, chip, board
tag, checksum, and SHA-256, then writes the inactive OTA partition before
switching to it. Do not remove power during that operation.

1. Confirm the exact model and compare the downloaded image's SHA-256 with the
   `SHA256SUMS` entry from the release.
2. Copy the matching `crossrsvp-<model>-v0.6.0.bin` image from the release bundle
   to the SD card.
   Do not select a neighbouring model's image.
3. On the reader open **Settings → System → SD Card Firmware Update**, select
   the image, wait for validation, and confirm the update.
4. Leave the reader powered until it reports completion and restarts. The
   updater revalidates the removable file immediately before writing.
5. Open an EPUB and perform the device checklist in [the audit](rsvp-device-audit.md).
   RSVP starts from the existing reader position and is entered explicitly.

The X4 Pro's USB MSC transfer mode temporarily owns the SD card. Finish copying
files, then use the computer's safe-eject/unmount command and wait for the host
to release the drive. Only after that, exit USB Transfer on the reader and wait
for its reboot before unplugging the cable or editing the card again. The C3
release profiles do not enable USB MSC.

## Rollback

Rollback uses the same SD-card update flow: verify the saved image's SHA-256,
select it by its exact model label, and wait for a complete reboot. Restore the
backed-up SD contents after the rollback image has booted.

If the normal UI is unavailable, the firmware has a model-specific recovery
hatch. On X3 and original X4, hold logical **Up** while pressing Power to
wake; on X4 Pro, hold logical **Down** while pressing Power. The firmware logs
the recovery mode and opens the same SD firmware picker. The Pro's Down key is
GPIO7; it is used because its Up key is the ESP32-S3 strap GPIO0. Recovery is
still subject to the firmware reaching its input initialization; it is not a
ROM or bootloader rescue path. If that hatch cannot be reached, stop rather
than trying an image for another model and use the project's supported service
recovery procedure.

The release artifact directory is generated after a successful build. It is
the source of truth for the image filename and checksum.

For a reproducible qualification build, use the repository's generated local
PlatformIO override with `extra_configs` blank (for example,
`platformio.qualification.local.ini`) so a developer's ignored
`platformio.local.ini` cannot silently change the dependency revision or build
flags. Keep that override outside Git and record its effective dependency
revision with the build report.
