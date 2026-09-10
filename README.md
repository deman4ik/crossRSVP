# CrossPoint Reader

> [!WARNING]
> CrossRSVP v0.6.0 is prepared for Xteink X3, the original X4, and X4 Pro. Physical
> qualification is still pending. Read the [device audit](docs/rsvp-device-audit.md)
> and [v0.6.0 installation and rollback guide](docs/rsvp-release-installation.md)
> before flashing. The [RSVP guide](docs/rsvp.md) covers button and touch controls.

## Что добавлено в форке crossRSVP

crossRSVP основан на CrossPoint Reader и сохраняет обычный постраничный режим, библиотеку, настройки и остальные
возможности базовой прошивки. CrossRSVP v0.6.0 добавляет режим RSVP для EPUB на Xteink X3, оригинальном X4 и X4 Pro:

| Возможность | Базовый CrossPoint | crossRSVP |
| --- | --- | --- |
| Открытие EPUB | Постраничный режим | Постраничный режим; RSVP включается только явно |
| Быстрое последовательное чтение | Focus Reading внутри страницы, без показа по одному слову | Отдельный взаимоисключающий режим RSVP: одна Presentation Group на кадр |
| Точка фиксации | Не применяется к последовательному показу слов | Unicode-безопасный ORP, жирная опорная буква и неподвижные направляющие |
| Группировка коротких слов | Нет | Опционально показывает Active Word и до двух целых слов-компаньонов, не сдвигая Active Word и ORP |
| Русский текст | Обычный постраничный рендеринг | ORP по символам, а не байтам UTF-8; поддержка `ё`, `й`, кавычек, тире и пауз по пунктуации |
| Переключение режимов | Не требуется | Paged ↔ RSVP с общей позицией; при группировке возврат продолжается после всей подтверждённой группы |
| Изображения и таблицы | Показываются на странице | RSVP останавливается: объект можно открыть в Paged или явно пропустить одной кнопкой |
| Восстановление позиции | Обычный прогресс страницы | Последнее подтверждённое экраном RSVP-слово переживает выход, перезапуск и очистку производного кэша; при открытии Paged оно снова подсвечивается, а явная Paged-навигация заменяет эту позицию |

Focus Reading из базовой прошивки остаётся отдельной функцией постраничного режима и не заменяется RSVP. Она доступна
в **Настройки → Чтение** и во вложенном **Настройки текста → Стиль**; обе строки меняют одну настройку.
Физическая квалификация v0.6.0 ещё не заявлена. Образы подписаны моделью: X3 и оригинальный X4 используют разные
имена пакетов, хотя работают на общем ESP32-C3 профиле; X4 Pro использует отдельный ESP32-S3 профиль с PSRAM,
сенсором, передней подсветкой, USB MSC и SDMMC.

| Модель | Управление RSVP | Профиль памяти и хранения |
| --- | --- | --- |
| Xteink X3 | Кнопки; наклон остаётся системной функцией X3 | ESP32-C3 без PSRAM, SD по SPI |
| Оригинальный X4 | Кнопки | ESP32-C3 без PSRAM, SD по SPI |
| X4 Pro | Кнопки навигации, Home и сенсорная RSVP Control Panel | ESP32-S3 с PSRAM, SDMMC; USB MSC доступен в профиле |

### Как включить и использовать RSVP

1. Установите образ своей модели по [инструкции v0.6.0](docs/rsvp-release-installation.md) и откройте EPUB.
   Книга всегда сначала открывается в постраничном режиме.
2. На X3/X4 коротко нажмите логическую кнопку **Подтвердить**; на X4 Pro коснитесь центра страницы.
   В открывшемся меню читалки выберите **Режим чтения RSVP**. В стандартной панели
   инструментов X4 Pro сначала откройте **More**.
3. Для быстрого кнопочного входа на X3/X4 один раз настройте **Настройки → Кнопки → Долгое нажатие меню → Режим чтения RSVP**. После
   этого удерживайте логическую кнопку **Подтвердить** примерно 0,4 секунды прямо во время чтения.
4. RSVP откроется на паузе со словом из текущей позиции чтения. На X3 и X4 управление использует логические
   кнопки и учитывает переназначение. На X4 Pro касание рабочей области во время воспроизведения ставит RSVP на
   паузу и открывает Control Panel; продолжение требует явной команды панели.

Опция **Настройки → Чтение → Группировать короткие служебные слова** по умолчанию выключена. При включении кадр может содержать
Active Word и один или два целых слова-компаньона. Группа не пересекает границы предложения, абзаца, главы и non-text
содержимого; не поместившееся слово-компаньон показывается отдельно и не теряется.

| Кнопка в RSVP | Действие |
| --- | --- |
| **Подтвердить** | Пуск/пауза |
| **Назад** | Вернуться в постраничный режим к последнему показанному слову |
| **Предыдущая страница** | Вернуться на пять групп и остаться на паузе |
| **Следующая страница** | На паузе показать следующую группу; на предупреждении пропустить один non-text объект |
| **Влево / Вправо** | Изменить скорость на −10/+10 слов в минуту в пределах настройки |
| Долгое **Назад** | Выйти на домашний экран или в проводник согласно настройке навигации |

Диапазон начинается с **60 слов/мин**, шаг — 10, значение по умолчанию — 100. Предварительный максимум —
**130** для X3/UC8253, **120** для X4 и X4 Pro/SSD1677, **100** для неизмеренных ревизий контроллера.
[Основания и ограничения расчёта](docs/rsvp-display-limits.md). Это запрашиваемый темп: синхронное
обновление E‑Ink входит в интервал каждого слова. Если панель обновляется дольше заданного интервала, RSVP показывает
следующее слово сразу после её готовности, но не пропускает слова и не пытается догнать таймер.

Во время блокирующего обновления E‑Ink нажатия не теряются: RSVP сохраняет Pause, переход в постраничный режим,
изменение темпа и навигацию в фиксированном буфере без динамического выделения памяти, а затем применяет их после
освобождения рендера. Поэтому на X3 реакция может появиться только после текущего физического refresh, но кнопки не
должны «зависать» при высокой запрошенной скорости.

На сообщении **«Для этого содержимого нужен постраничный режим»** нажмите **Следующая страница**, чтобы пропустить
одно изображение, таблицу, разделитель или другой неподдерживаемый объект и увидеть следующее слово на паузе. Нажмите
**Назад**, если объект нужно посмотреть в контексте страницы. Непомещающееся слово намеренно не пропускается — для
него доступен только постраничный режим.

Полная карта кнопок, сенсорной панели, последовательных объектов, глав, переназначения и проверки на устройстве
описаны в [руководстве RSVP](docs/rsvp.md). Результаты автоматической проверки и её ограничения приведены
в [отчёте v0.6.0](docs/rsvp-v0.6.0-validation.md), а образы и контрольные суммы —
в [release notes](docs/rsvp-release-notes.md). До физических испытаний выпуск помечен как pre-release.

[![Fund contributors](https://img.shields.io/badge/%F0%9F%91%91_Fund_contributors-royalty.dev-BB953A?style=for-the-badge&labelColor=1a1a1a)](https://app.royalty.dev/crosspoint-reader/crosspoint-reader)

CrossPoint is open-source e-reader firmware - community-built, fully hackable, free forever. It's maintained by a growing community of developers and readers who believe your device should do what you want - not what a manufacturer decided for you.

### Now running on:
- **ESP32C3-based** Xteink X4 and X3.
- **ESP32S3-based** Xteink X4Pro, Seeed reTerminal Sticky, M5PaperMono

Check [our Devices page](https://crosspointreader.com/devices) for the full list.

![CrossPoint Reader running on Xteink device](./docs/images/cover.jpg)

> If you're planning to buy an Xteink device, consider purchasing an **X3/X4 Developer Edition** through https://crosspointreader.com. CrossPoint receives a small share of each sale, helping fund development costs.

## What can CrossPoint do?

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, adaptive table layouts, native CJK ruby annotations, chapter navigation, footnotes, bookmarks, dictionary lookups ([StarDict](docs/dictionary.md)), go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync and more.

- **Various formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.

- **Touch reading**: follow EPUB links and look up words in the dictionary on touch-enabled devices.

- **Screenshots.**

- **Custom fonts**: install your favorite fonts on the SD card.

- **Tilt page turn (X3 and Sticky)**.

- **USB Drive mode (X4Pro)**: access the SD card as USB mass storage.

- **Library workflow**: folder browser, hidden-file toggle, long-press delete, recent books, SD-cache management.

- **Wireless workflows**:
  
  - File transfer web UI
  - EPUB Optimizer
  - Web settings UI/API (edit many device settings from browser)
  - WebSocket fast uploads
  - WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - OTA update checks and installs from GitHub releases

- **Customization**: night mode, multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep screen modes including transparent overlays, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: 34 UI languages and counting, including CJK font fallback and RTL support.

### Coming soon:

- More themes.

- Web plugins.

- Bluetooth pageturner.

- Much more! stay tuned.

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool available at
https://crosspointreader.com/#unlock-tool before you can flash CrossPoint.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
> 
> **The only officially supported firmwares in the unlock tool are CrossPoint and CrossInk.**
> 
> Flashing any other firmware on a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path**. Once USB flashing is re-locked, your only way back is via OTA, and if
> the firmware you flashed doesn't support OTA, **there is no way out**.

## Install firmware

### Web installer (recommended)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Go to https://crosspointreader.com/#flash-tools, select your device (X3, X4, Xteink X4Pro, Seeed reTerminal Sticky, or M5PaperMono), and choose an official CrossPoint release.

### Web installer (specific version)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download the firmware file for your device from [Releases](https://github.com/crosspoint-reader/crosspoint-reader/releases), or compile yourself.
3. Go to https://crosspointreader.com/#flash-tools, select your device, click "Custom .bin" and upload the firmware file.

### Revert to Official Firmware

To revert to the official firmware, you can also flash the latest official firmware using https://crosspointreader.com/#flash-tools.

### Command line

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download the firmware file for your device from the [releases page](https://github.com/crosspoint-reader/crosspoint-reader/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash an X3 or X4:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

   Flash an Xteink X4Pro, Seeed reTerminal Sticky, or M5PaperMono:

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

### Manual

See [Development quick start](#development-quick-start) below.

---

## Custom SD-card fonts

Convert your own TTF/OTF files into `.cpfont` files that load from the SD card. No firmware reflash is needed.

1. Go to https://crosspointreader.com/fonts and open the "SD-card font builder" form.
2. Upload up to four styles (regular, bold, italic, bold-italic), set the family name, point sizes, and Unicode range.
3. Download the generated `.cpfont` files.
4. Copy them to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
5. Select the font on the device from the font settings.

Conversion runs the firmware repo's `lib/EpdFont/scripts/fontconvert_sdcard.py` script unmodified, so output matches a local host build.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)
- [Touch and UI development](./docs/contributing/touch-and-ui.md) - how to build new screens on the FreeInkUI activity bases (UiListActivity and friends), plus build envs for the non-Xteink touch devices

---

## Development quick start

### Prerequisites

- [pioarduino PlatformIO Core](https://github.com/pioarduino/platformio-core) or [VS Code + pioarduino IDE](https://github.com/pioarduino/pioarduino-vscode-ide)
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive https://github.com/crosspoint-reader/crosspoint-reader
cd crosspoint-reader

# if cloned without --recursive:
git submodule update --init --recursive
```

### Nix/NixOS

Nix/NixOS users can enter the development shell with either `nix develop` (flakes) or `nix-shell`:

```bash
nix develop -f nix
# or
nix-shell nix
```

To flash a connected ESP32-C3 device, enable PlatformIO's udev rules in your NixOS configuration:

```nix
services.udev.packages = with pkgs; [ platformio-core.udev ];
```

After rebuilding the system configuration, reconnect the device or reload udev rules.

### Build / flash / monitor

```bash
pio run --target upload
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

CrossPoint Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the
cache. This cache directory exists at `.crosspoint` on the SD card. The structure is as follows:

```text
.crosspoint/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   ├── css_rules.cache  # parsed CSS rule cache
│   ├── img_*            # rendered image cache files
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── settings.json        # device settings
├── state.json           # resume/runtime state
└── recent.json          # recent books list
```

Removing `/.crosspoint` clears all cached metadata and forces a full regeneration on next open. Book deletes, overwrites, and moves done through the firmware or web UI clear or re-key matching caches; manual SD-card edits may leave stale cache directories behind.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md). For things to work on, check the [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas) — leave a comment before starting so we don't duplicate effort.

Everyone here is a volunteer, so please be respectful and patient. For governance and community expectations, see [GOVERNANCE.md](./GOVERNANCE.md).

---

## Community forks

One of the best things about open source is that anyone can take the code in a different direction. If you need something outside CrossPoint's [scope](./SCOPE.md), check out the community forks:

- [CrossInk](https://github.com/uxjulia/CrossInk) — UX focused with minimal reading stats and broader customizations for the reading experience.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) — More faithful CSS styling and background work for slightly snappier interaction. Weather information panel. Markdown support.

**Note:** Many of these features will make their way into CrossPoint over time. Each project chooses its own priorities and tradeoffs.

Want to build your own device? Be sure to check out the [de-link](https://github.com/iandchasse/de-link) project or [OnePage Reader](https://github.com/MoveCall/onepage-reader).

---

CrossPoint Reader is **not affiliated with Xteink or any device manufacturer**.
