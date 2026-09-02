# RSVP Reading Mode v0.1 Implementation Plan

Status: approved and published to GitHub Issues.

## Baseline

- Repository: `deman4ik/crossRSVP`
- Branch: `develop`
- Baseline commit: `7bcc3a692231445f870ea524bea0e14cd0a05300`
- Target: Xteink X3, EPUB, Russian and Latin text
- Domain model: [`CONTEXT.md`](../../CONTEXT.md)
- Decisions: [`ADR-0001`](../adr/0001-base-the-fork-on-crosspoint.md), [`ADR-0002`](../adr/0002-switch-exclusive-modes-at-a-shared-position.md), [`ADR-0003`](../adr/0003-traverse-document-elements-and-checkpoint-rsvp-separately.md)
- Product specification: [`spec 0001`](../specs/0001-rsvp-reading-mode-x3.md)
- Parent tracker issue: [#1](https://github.com/deman4ik/crossRSVP/issues/1)
- Implementation issues: [#2](https://github.com/deman4ik/crossRSVP/issues/2), [#3](https://github.com/deman4ik/crossRSVP/issues/3), [#4](https://github.com/deman4ik/crossRSVP/issues/4), [#5](https://github.com/deman4ik/crossRSVP/issues/5)

## Current Architectural Seams

- `ReaderActivity::create` is the single format factory and creates one heap-owned reader activity (`src/activities/reader/ReaderActivity.cpp:27`). It is the narrow routing point for EPUB Paged/RSVP selection.
- `ActivityManager::replaceActivity` defers safe teardown/replacement and owns the mutually exclusive screen lifecycle (`src/activities/ActivityManager.cpp:214`). `goToReader` already delegates to the reader factory (`src/activities/ActivityManager.cpp:265`).
- `EpubReaderActivity` already tracks current spine, page, and visible-text offsets (`src/activities/reader/EpubReaderActivity.h:20`, `src/activities/reader/EpubReaderActivity.h:23`, `src/activities/reader/EpubReaderActivity.h:29`). Its progress save resolves page starts to visible Unicode offsets (`src/activities/reader/EpubReaderActivity.cpp:1476`).
- `Section` maps visible Unicode offsets in both directions without rebuilding a second position system (`lib/Epub/Epub/Section.h:151`, `lib/Epub/Epub/Section.cpp:990`, `lib/Epub/Epub/Section.cpp:1027`).
- `ProgressMapper` already streams EPUB spine XHTML under a fixed 1 KB input chunk through `ParagraphStreamer` (`lib/KOReaderSync/ProgressMapper.cpp:199`, `lib/KOReaderSync/ProgressMapper.cpp:766`). The parser is currently private to that translation unit and is the strongest prefactoring candidate for a reusable source-ordered visible-content stream.
- Paginated `Page` objects distinguish lines, images, and horizontal rules and carry page-start visible offsets (`lib/Epub/Epub/Page.h:15`, `lib/Epub/Epub/Page.h:34`, `lib/Epub/Epub/Page.h:47`, `lib/Epub/Epub/Page.h:76`). They are useful for mapping and Paged highlighting but are insufficient as the sole RSVP source because per-word offsets are discarded and table semantics are flattened.
- `TextBlock` offers compact NUL-terminated word access with one arena allocation (`lib/Epub/Epub/blocks/TextBlock.h:41`, `lib/Epub/Epub/blocks/TextBlock.h:92`). It is suitable for rendering a mapped page, not for durable word identity.
- `ParsedText` normalizes words to NFC and tracks visible offsets before layout (`lib/Epub/Epub/ParsedText.cpp:390`). Existing Focus Reading code is reusable as Unicode and glyph-measurement prior art but emphasizes a prefix rather than one ORP pivot (`lib/Epub/Epub/ParsedText.cpp:503`).
- `GfxRenderer::getTextAdvanceX` matches actual glyph, kerning, BiDi, and combining-mark behavior (`lib/GfxRenderer/GfxRenderer.cpp:2011`). RSVP rendering should compose existing measurement/draw primitives rather than add a low-level display API.
- The classic EPUB menu is assembled in one bounded list (`src/activities/reader/EpubReaderMenuActivity.cpp:40`). Its new Mode Switch action must remain within `MAX_MENU_ITEMS`.
- Long-Confirm actions are persisted by numeric index and may only be appended (`src/CrossPointSettings.h:144`).
- `PersistableStore` provides serialized JSON access but is global-store oriented (`lib/Serialization/PersistableStore.h:19`). The per-book binary checkpoint should instead follow existing cache-file patterns through `HalStorage`, with explicit versioning and atomic replacement.
- Host-side tests use Google Test through the root test CMake project (`test/CMakeLists.txt:1`). Existing Unicode, combining-mark, token-boundary, differential-rounding, and cache tests provide conventions to copy.

## Primary Testing Seam

Introduce one testable RSVP session/controller between the Activity and low-level helpers:

```text
logical X3 action + monotonic time
                │
                ▼
          RSVP session
        ┌───────┼────────┐
        ▼       ▼        ▼
 visible EPUB  display   checkpoint/
 event stream  command   progress decision
```

The host test drives this seam with a small real EPUB fixture, fake time, recorded display commands, and temporary storage. It verifies the end-to-end behavior that matters—word order, ORP coordinates, timing, Mode Switch position, boundaries, and recovery—without emulating FreeRTOS or the physical panel. Pure helpers remain separately tested only where they have dense edge-case matrices.

## Proposed Components

### Reusable visible-content stream

- Extract the reusable visible-text/entity/tag handling currently embedded in ProgressMapper.
- Emit bounded events for lexical text, paragraph/chapter boundaries, and non-text Document Elements.
- Preserve zero-based visible Unicode offsets and normalize text through existing UTF-8 facilities.
- Stream one spine at a time from the EPUB source; never collect a whole chapter or book.
- Keep ProgressMapper behavior unchanged and migrate it to the shared stream as a regression proof.

### RSVP document cursor

- Own the current spine, visible offset, token disambiguation, and event state.
- Seek forward from a Resume Anchor and provide one reconstructed token at a time.
- Rejoin layout/discretionary hyphenation, retain internal hyphens, and classify surrounding punctuation.
- Detect chapter and unsupported Document Element boundaries before advancing past them.
- Translate a current visible offset to a Paged page through `Section` mapping.

### RSVP session/controller

- Own Paused/Playing/Boundary/Error state, current token, Playback Pace, next frame deadline, and checkpoint-dirty state.
- Consume logical commands: play/pause, rewind five, step one, pace ±10, Mode Switch, and exit.
- Never skip tokens to recover timing. Refresh duration consumes the base interval; linguistic pauses extend it.
- Request checkpoint writes on the approved events and at the 30-second debounce boundary.
- Produce explicit pause/fallback reasons for chapter, image/table, oversized word, corruption, and fatal load failure. PageForward may consume one recoverable non-text boundary and expose the next word paused; oversized words remain Paged-only.

### RSVP renderer

- Derive Lexical Core and ORP byte boundaries with Unicode-safe iteration.
- Measure prefix and pivot using the same style and glyph APIs used for drawing.
- Align the pivot center to a fixed orientation-aware focus coordinate.
- Draw prefix, bold pivot, suffix, and static guide marks on one baseline.
- Reduce the active font size for oversized words without changing global settings.
- Use full-frame fast refresh and a calibrated cleanup cadence; do not introduce partial refresh.

### Reader activities and controls

- Route EPUB creation to Paged or RSVP through the reader factory using explicit launch intent, not a global boolean that can produce contradictory state.
- Add a menu Mode Switch and append an RSVP value to long-Confirm configuration without shifting persisted enum values.
- Keep short Confirm as play/pause, PageBack as five-word rewind, PageForward as paused single-step or an explicit one-element non-text skip, Left/Right as pace control, Back as Paged fallback, and long-Back as existing Home/File Browser navigation.
- Enter RSVP paused and always open a newly selected book in Paged Mode.
- On RSVP → Paged, resolve the anchor to a page and request a one-shot word highlight.
- On Paged → RSVP, repeat the last RSVP token when no page turn occurred; otherwise start from the displayed page's first visible offset.

### Settings, i18n, persistence, and statistics

- Add bounded global settings with defaults: 100 WPM, 60–120 range, 10-WPM steps, visible ORP guides, 1.5×/2×/2.5× pauses.
- Add English source strings and Russian translations; regenerate ignored i18n outputs only for local verification.
- Store a versioned RSVP Checkpoint inside the book cache using `HalStorage`; validate Book Revision and reject corrupt, stale, or unsupported versions.
- Save atomically every 30 seconds and on pause, Mode Switch, chapter transition, sleep, and exit, while suppressing unchanged writes.
- Advance native page progress only when the Resume Anchor crosses a page boundary and include active RSVP time in existing statistics.

## Delivery Sequence

The tracker work is intentionally compressed into four large vertical slices. Four is the minimum practical count that keeps each issue independently verifiable and small enough for one fresh implementation context.

1. **Deliver a calibrated paused RSVP tracer on X3.** Extract the reusable visible EPUB stream, establish the `RsvpSession` seam, route an EPUB into a separate RSVP Activity, show the first source token on pause, and include the timing instrumentation needed to publish the safe X3 refresh/pacing budget.
2. **Complete Russian ORP playback and X3 controls.** Add Unicode Lexical Core parsing, pixel-metric ORP, deadline playback, punctuation and structural pauses, oversized-word handling, chapter/non-text boundaries, logical X3 controls, global settings, Russian/English i18n, and cleanup-refresh scheduling.
3. **Complete seamless Mode Switching and durable Reading Position.** Add Paged ↔ RSVP switching, temporary Paged highlighting, repeat/restart semantics, versioned checkpoints, Book Revision validation, sleep/exit recovery, native progress/statistics reconciliation, and recoverable/fatal fallback behavior.
4. **Qualify and release experimental X3 v0.1.** Run the complete host, simulator, firmware-build, and 30-minute X3 acceptance matrix; record timing/heap/battery results and publish backup/rollback documentation.

## Verification

### Automated

- Configure host tests: `cmake -S test -B build/test`
- Build host tests: `cmake --build build/test`
- Run host tests: `ctest --test-dir build/test --output-on-failure -j`
- Format changed C/C++ only through the repository wrapper: `./bin/clang-format-fix -g`
- Run static analysis where relevant: `pio check`
- Build the relevant firmware target once after final code changes: `pio run`
- Verify ignored/generated files are not staged.

### Simulator

- Open the RSVP tracer fixture in all four orientations.
- Capture paused word, playing word, boundary prompt, post-skip word, quick long-Confirm entry, fatal fallback, and Paged highlight states.
- Verify logical button actions and no flash of Paged content while RSVP is active.

### Xteink X3

- Record fast and cleanup refresh duration distributions before setting the final pace cap/cadence.
- Run a 30-minute Russian EPUB at 100 WPM.
- Verify exact token order, interval error ≤10% outside deliberate pauses, readable ghosting, and free heap >50 KB with no declining trend.
- Exercise every save trigger, sleep/resume, corrupted checkpoint, changed Book Revision, chapter boundary, image/table, long word, and fatal fallback.
- Verify every supported orientation and backup/rollback procedure.

## Risks and Controls

- **Refresh latency dominates WPM:** calibrate first, cap honestly, and never skip words.
- **Heap fragmentation:** stream bounded events, reuse fixed buffers, use `makeUniqueNoThrow`, and instrument heap during the hardware run.
- **Position drift across layout changes:** use visible Unicode offsets as the semantic anchor and page LUTs only for projection into Paged Mode.
- **Flattened table/image semantics in page cache:** derive RSVP events from source order, not only paginated `TextBlock` objects.
- **Persisted enum compatibility:** append the long-Confirm action and test old settings JSON.
- **SD latency/wear:** debounce, suppress unchanged writes, and use atomic checkpoint replacement through `HalStorage`.
- **Upstream churn:** hold the pinned baseline through v0.1 and integrate stable releases afterward.

## Definition of Done

- Every in-scope user story in spec 0001 is demonstrated by an automated, simulator, or X3 acceptance test.
- No out-of-scope format, device, multi-word mode, or partial-refresh path is introduced.
- Host tests, formatting, static analysis, and firmware build pass.
- X3 acceptance results and battery observation are recorded.
- Russian and English UI strings are complete.
- Checkpoint format and rollback instructions are documented.
- No generated or local CodeGraph files are staged.
