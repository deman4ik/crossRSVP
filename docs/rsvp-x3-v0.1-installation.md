# Install and roll back crossRSVP X3 v0.1

This procedure is for the **experimental crossRSVP v0.1 release candidate qualified only for Xteink X3 / EPUB**. It has been built
and exercised in the X3 simulator, but the backup, flash, endurance, and rollback steps still require execution on a
physical X3. The image uses the shared X3/X4 ESP32-C3 board profile, but X4 has not been qualified; do not install this
candidate on X4, X4 Pro, X4C, Sticky, or another device.

## Candidate identity

- File: `artifacts/rsvp-x3-v0.1/firmware/crossrsvp-x3-v0.1-experimental.bin`
- Download: [experimental X3 pre-release](https://github.com/deman4ik/crossRSVP/releases/download/v0.1.0-crossrsvp-x3/crossrsvp-x3-v0.1-experimental.bin)
- Size: 5,425,152 bytes
- SHA-256: `c4d3fe981bb5fb9e5e5bb42e200e536d22bae14ee9ac7d9bae235acae05c20eb`
- Embedded version: `0.1.0-crossrsvp-experimental-x3`

Verify the file before copying or flashing it:

```bash
shasum -a 256 artifacts/rsvp-x3-v0.1/firmware/crossrsvp-x3-v0.1-experimental.bin
```

Stop if the hash differs.

If the named local file is absent, download the permanent pre-release asset above or create it and its checksum with
`pio run -e rsvp_x3_release -j 1`. The environment's post-build step packages
`.pio/build/rsvp_x3_release/firmware.bin` under `artifacts/`. A GitHub Actions run of
**Build experimental crossRSVP X3 candidate** produces a temporary artifact with a checksum for that specific build.

## 1. Back up before flashing

1. Charge the X3 and keep it connected to stable USB power.
2. Power the reader down, remove the SD card, and copy the entire card to a dated backup directory. Include hidden
   files and the complete `/.crosspoint` directory; it contains caches, progress, and RSVP checkpoints.
3. If the currently installed firmware `.bin` is available, save it beside the SD backup and record its version and
   SHA-256. This is the simplest rollback image.
4. For a complete recovery backup, connect an unlocked X3 by USB, replace the port below, and read all 16 MiB of flash:

```bash
pio pkg exec -p tool-esptoolpy -- esptool.py \
  --chip esp32c3 -p /dev/cu.usbmodemXXXX -b 921600 \
  read_flash 0x000000 0x1000000 x3-flash-backup.bin
shasum -a 256 x3-flash-backup.bin
```

Store the flash image and its hash away from the SD card. A USB-locked unit may not permit this step; do not use an
unlock workflow unless you understand the device vendor's restrictions.

## 2. Install the candidate

### From an existing CrossPoint-compatible firmware

This is the preferred path because it uses the firmware's board validation and OTA partition update flow:

1. Copy `crossrsvp-x3-v0.1-experimental.bin` anywhere on the X3 SD card.
2. Insert the card and start the reader.
3. Open **Settings → System → SD Card Firmware Update**.
4. Select the `.bin`, review the confirmation, and start the update.
5. Keep power connected and do not press buttons, remove the card, or power off until the device restarts.

The picker accepts `.bin` files, validates the image header and OTA partition size, streams it to the inactive OTA
partition, and restarts only after a successful write.

### Custom web flasher

For an X3 that is already USB-flashable, connect it to Chrome or Edge and use the CrossPoint web installer's
**Custom .bin** option at <https://crosspointreader.com/#flash-tools>. Select X3 and upload the candidate file. Do not
select X4. The normal upstream OTA updater follows upstream releases and will not install this fork's local candidate.

## 3. Verify the installation

1. Confirm the device boots normally and reports `0.1.0-crossrsvp-experimental-x3` in its version/log output.
2. Open an EPUB. It must start in Paged Mode; RSVP must never start automatically.
3. Follow the [Russian RSVP button guide](rsvp-controls-ru.md): configure the long-Confirm shortcut, enter RSVP directly,
   and confirm the first word is visible while paused.
4. Check play/pause, pace changes, rewind five words, and single-word advance.
5. At an image or table prompt, press **Next Page** once. RSVP must show the next word paused without entering Paged
   Mode. Press **Back** separately to verify that Paged Mode remains available for viewing the skipped content.
6. Confirm the last RSVP word is highlighted and re-entering RSVP without a page turn repeats it.
7. Stop and roll back if the reader resets, cannot return to Paged Mode, clips or skips a word unexpectedly, or loses
   the reading position.

## 4. Roll back

### Normal rollback

1. Copy the previously saved X3 firmware `.bin` to the SD card.
2. Open **Settings → System → SD Card Firmware Update** and install it exactly as above.
3. After restart, verify its version and open an EPUB in Paged Mode.

### Boot recovery picker

If the normal UI does not start but the bootloader and SD reader still work:

1. Put a known-good X3 `.bin` on the SD card and insert it.
2. With the X3 powered off, hold the **left-side button** while pressing Power.
3. Keep the side button held until the recovery firmware picker appears, then select the known-good image.
4. Keep power stable until restart.

### USB recovery

For an unlocked device, use the CrossPoint web installer to flash a known-good official X3 release. The 16 MiB readback
above is an emergency/forensic backup, not a normal USB rollback image. A complete-flash recovery may require an
external SPI programmer and is destructive; preserve the backup and follow the project's
[bricked-device recovery notes](fix-bricked-xteink.md) instead of inventing an `esptool write_flash` command.

## 5. Reset reading cache

Back up `/.crosspoint` first if reading progress or RSVP checkpoints must be preserved. Then either:

- use **Settings → System → Clear Reading Cache**; or
- with the device powered down and SD card mounted on a computer, remove only the affected
  `/.crosspoint/epub_<hash>/` directory.

Do not delete the entire SD card or an unverified directory. Reopen the EPUB in Paged Mode and allow its cache to be
rebuilt before enabling RSVP.

## Physical X3 record sheet

Record results against the exact candidate SHA-256. A blank or failed blocking row means the candidate remains
simulator-qualified only.

| Check | Required result | Actual result / evidence |
| --- | --- | --- |
| SD backup and restore sample | Backup readable; hidden `/.crosspoint` present | Pending |
| Full flash backup | 16 MiB file and SHA-256 recorded, or USB-lock limitation documented | Pending |
| Install | Candidate hash and embedded version match | Pending |
| 30-minute Russian EPUB at 100 WPM | No loss/reorder, crash, reset, or exhaustion | Pending |
| Ordinary frame intervals | Within ±10% outside deliberate pauses | Pending |
| Refresh and cleanup | Distribution captured; cleanup skips no word | Pending |
| Heap | Above 50 KB with no declining trend | Pending |
| Ghosting | Readable for the full run | Pending |
| Battery | Start/end level and elapsed time recorded | Pending |
| Controls and mode switching | All controls, quick entry, non-text skip, highlight, repeat, and page-turn restart pass | Pending |
| Checkpoint triggers | 30 s, pause, switch, chapter, sleep, and exit pass | Pending |
| Recovery data | Sleep, revision change, corrupt checkpoint, progress, and statistics pass | Pending |
| Content/failure cases | Chapter, image/table, long word, and fatal fallback pass | Pending |
| Orientations | All four pass | Pending |
| Rollback | Known-good firmware restored and EPUB opens | Pending |
| Cache reset | Cache rebuilds; EPUB starts in Paged Mode | Pending |

Attach serial logs, interval data, heap samples, battery readings, photos, and the completed table to the release issue.
