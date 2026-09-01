# RSVP Reading Mode for Xteink X3

## Problem Statement

An Xteink X3 reader can read EPUB books in Paged Mode, but cannot switch to a focused Rapid Serial Visual Presentation experience that keeps the eye fixed on an Optimal Recognition Point. Existing experimental RSVP firmware replaces Paged Mode outright, centers whole words instead of their ORP, handles UTF-8 and Russian punctuation incompletely, and reports a nominal speed that does not account for E-Ink refresh time. A reader who misses a word cannot quickly recover surrounding context and then continue RSVP from the same Reading Position.

## Solution

Add an explicitly enabled RSVP Mode for EPUB books on Xteink X3. Paged Mode and RSVP Mode remain separate, mutually exclusive reading activities, but a Mode Switch transfers a shared Resume Anchor between them. RSVP presents one word per frame, aligns a bold ORP letter between fixed guide marks, supports Russian and Latin text, exposes a physically achievable Playback Pace, and safely pauses at chapter boundaries or Document Elements that require Paged Mode.

The v0.1 release is experimental. It uses the existing full-frame refresh path, persists a versioned RSVP Checkpoint independently from native page progress, starts every newly opened book in Paged Mode, and retains a safe route back to Paged Mode on unsupported content or failure.

## User Stories

1. As an X3 reader, I want to enable RSVP from the in-reader menu, so that the feature is discoverable without changing firmware settings elsewhere.
2. As an X3 reader, I want to assign long-Confirm to the Mode Switch, so that I can move between Paged Mode and RSVP Mode without navigating a menu.
3. As an X3 reader, I want only one Reading Mode active at a time, so that page rendering and RSVP playback cannot conflict.
4. As an X3 reader, I want RSVP to open paused with its first word visible, so that playback never starts before I am ready.
5. As an X3 reader, I want short Confirm to play and pause RSVP, so that transport control is predictable.
6. As an X3 reader, I want Back to return immediately to Paged Mode, so that I can recover context after missing a word.
7. As an X3 reader, I want long-Back to retain the existing Home or File Browser behavior, so that RSVP does not trap me inside a book.
8. As an X3 reader, I want PageBack to rewind five words, so that I can replay a short missed passage.
9. As an X3 reader, I want PageForward to advance one word while paused, so that I can inspect difficult text precisely.
10. As an X3 reader, I want Left and Right to change Playback Pace in 10 WPM steps, so that speed remains adjustable during reading.
11. As an X3 reader, I want a default Playback Pace of 100 WPM, so that the first experience is realistic for the X3 panel.
12. As an X3 reader, I want selectable paces from 60 to 120 WPM, so that I can trade speed for clarity.
13. As an X3 reader, I want refresh time included in the playback interval, so that the displayed WPM describes observable behavior rather than timer delay alone.
14. As an X3 reader, I want an unreachable pace capped safely, so that playback never skips words to catch up.
15. As a speed reader, I want every word aligned by its ORP, so that my fixation point remains stationary across words of different lengths.
16. As a speed reader, I want the ORP letter bold and framed by fixed upper and lower guide marks, so that the fixation point is visible on a monochrome display.
17. As a Russian reader, I want ORP calculated from Unicode characters rather than UTF-8 bytes, so that Cyrillic words align correctly.
18. As a Russian reader, I want `ё`, `й`, and canonically equivalent decomposed text handled correctly, so that normalization does not change reading behavior.
19. As a Russian reader, I want guillemets, quotation marks, brackets, dashes, ellipses, and punctuation displayed with their word but excluded from ORP selection, so that text remains faithful without shifting fixation.
20. As a Russian reader, I want clause punctuation to pause for 1.5 times the base interval, so that phrase structure remains perceptible.
21. As a Russian reader, I want sentence punctuation to pause for twice the base interval, so that sentence endings remain perceptible.
22. As a reader, I want paragraph boundaries to pause for 2.5 times the base interval, so that paragraph structure remains perceptible.
23. As a reader, I want punctuation before closing quotes or brackets to retain its pause, so that Russian dialogue is paced correctly.
24. As a reader, I want layout-only and discretionary hyphenation rejoined, so that a single lexical word is not presented as two words.
25. As a reader, I want internal hyphens to remain part of the Lexical Core and em dashes treated as boundaries, so that compounds and dialogue behave differently.
26. As a reader, I want oversized words reduced only as much as required, so that the full word remains visible whenever possible.
27. As a reader, I want playback to pause and offer Paged Mode when a word cannot fit at the minimum size, so that text is never clipped or silently altered.
28. As a reader, I want page boundaries to pass without interruption, so that pagination does not affect RSVP flow.
29. As a reader, I want playback paused at the beginning of a new chapter, so that semantic transitions are not missed.
30. As a reader, I want playback paused before images, tables, and other unsupported non-text Document Elements, so that meaningful book content is not silently skipped.
31. As a reader, I want the page containing the last RSVP word opened and that word highlighted after switching to Paged Mode, so that I can immediately find the missed context.
32. As a reader, I want a return to RSVP without a page turn to repeat the last RSVP word, so that the transition cannot skip text.
33. As a reader, I want a return to RSVP after a page turn to start at the first word of the displayed page, so that the device makes a safe choice when it cannot know my eye position.
34. As a reader, I want books to reopen in Paged Mode, so that RSVP always remains explicitly enabled.
35. As a reader, I want my RSVP Checkpoint saved every 30 seconds and on pause, Mode Switch, chapter transition, sleep, and exit, so that interruptions lose little progress.
36. As a reader, I want a changed Book Revision to invalidate its old RSVP Checkpoint, so that a saved offset never opens unrelated text.
37. As a reader, I want a corrupted checkpoint to fall back to native page progress, so that I can continue reading safely.
38. As a reader, I want RSVP reading time included in existing book statistics, so that my reading history remains coherent.
39. As a reader, I want native page progress updated as the Resume Anchor crosses pages, so that other CrossPoint features see a meaningful position.
40. As a Russian-speaking user, I want every new RSVP label and error localized in Russian with English fallback, so that the feature matches the rest of the firmware.
41. As an X3 user, I want the controls and ORP layout to work in every supported orientation, so that rotating the device does not change behavior.
42. As an X3 user, I want periodic ghost cleanup placed at natural boundaries where possible, so that the display stays readable without losing words.
43. As an X3 user, I want a clear error followed by a safe Paged Mode fallback after an unrecoverable RSVP failure, so that low memory or damaged content does not crash the reader.
44. As a firmware tester, I want observable timing, heap, checkpoint, and word-order diagnostics, so that v0.1 behavior can be verified on real hardware.
45. As a firmware installer, I want backup and rollback instructions, so that I can safely leave the experimental release.

## Implementation Decisions

- The fork is based on the pinned CrossPoint `develop` revision recorded in ADR-0001 and is released independently.
- RSVP v0.1 supports Xteink X3 and EPUB only.
- Paged Mode and RSVP Mode use separate activity lifecycles. The factory/router selects one screen-lifetime object, and a Mode Switch replaces the active activity instead of layering both state machines.
- A testable RSVP session/controller is the primary behavioral seam. It consumes logical reader actions and a source-ordered EPUB event stream, owns playback and Resume Anchor state, and emits display/checkpoint decisions. The hardware activity adapts mapped X3 input, renderer calls, clock, and storage to that session.
- The source stream uses the EPUB's visible Unicode offset as the semantic Reading Position. It exposes text, paragraph/chapter boundaries, and unsupported Document Elements without materializing an entire chapter in RAM.
- Existing progress mapping and visible-offset facilities are reused rather than creating a second unrelated location system.
- The RSVP Checkpoint is a separate, versioned, bounded per-book record. It contains Book Revision identity, spine identity, visible-text offset, token disambiguation data, and enough state to restore the Resume Anchor. It does not replace native page progress.
- Checkpoint writes use the storage HAL, are atomic, avoid redundant writes, and occur no more often than the approved checkpoint policy.
- Global settings include Playback Pace, RSVP font size, ORP guide visibility/style, and punctuation multipliers. Per-book RSVP settings are deferred.
- A new reader-menu action enables RSVP. A new append-only long-Confirm action value provides the optional direct Mode Switch without changing existing persisted enum meanings.
- Logical button mappings are used exclusively; raw X3 GPIO identities and same-ladder button chords are not used.
- The RSVP hot path keeps bounded reusable buffers. It does not create a full-book or full-chapter word collection, use repeated `std::string` construction, or perform fallible throwing allocation.
- ORP is computed from the normalized Lexical Core using codepoint/grapheme-safe byte boundaries. The initial one-based ORP positions are 1, 2, 3, 4, and 5 for lexical lengths 1–2, 3–4, 5–6, 7–8, and 9 or more.
- Rendering measures the actual prefix and pivot glyph advances. Prefix, bold pivot, and suffix are drawn as separate runs on a common baseline with orientation-aware dimensions and fixed guide marks.
- Playback uses frame deadlines. Display refresh duration consumes the base interval; the scheduler waits only for the remaining time and never skips tokens to catch up.
- Punctuation and structural pauses extend the base interval after the final significant Unicode punctuation, including punctuation followed by closing quotes or brackets.
- The existing full-frame fast refresh path is used. Experimental partial/window refresh is out of scope. Cleanup cadence is calibrated on X3 and prefers paragraph boundaries.
- Non-text content and unrenderable long words are first-class pause reasons. Recoverable cases offer a Mode Switch; fatal cases report an error and return to Paged Mode at the nearest safe Resume Anchor.
- All user-visible strings use the existing i18n pipeline with Russian translations and English fallback.
- Upstream integration is deferred until v0.1 is complete; later updates follow stable CrossPoint releases in explicit merge cycles.

## Testing Decisions

- Tests assert external behavior: displayed token/order, ORP pixel anchor, mode/position transitions, timing decisions, persisted recovery, and user-visible fallback. They do not assert private fields or incidental call order.
- The primary host-side seam is the RSVP session/controller driven by logical actions, a fake monotonic clock, a display-command recorder, checkpoint storage, and real small EPUB fixtures. This is the highest practical seam that avoids FreeRTOS and physical display dependencies.
- Source-stream tests cover visible offsets, paragraph/chapter boundaries, images/tables, soft hyphens, and bounded memory behavior using the existing host-side Google Test infrastructure.
- Unicode/ORP tests cover Russian precomposed and decomposed letters, quotes/brackets, dashes, ellipses, NBSP variants, numbers, abbreviations, proportional glyph measurements, and oversized words.
- Checkpoint tests cover atomic replacement, redundant-write suppression, corruption, version mismatch, Book Revision mismatch, and each required save event.
- Mode Switch tests cover RSVP-to-Paged highlighting, no-page-turn repetition, page-turn restart, chapter boundaries, and failure fallback.
- Simulator checks cover every supported orientation and provide deterministic screenshots for paused RSVP and error/pause states.
- X3 acceptance requires a 30-minute Russian EPUB run at 100 WPM with no lost/reordered words, crashes, or memory exhaustion; ordinary interval error stays within 10%, ghosting remains readable, Mode Switch preserves position, and sleep restores the checkpoint.
- Heap monitoring must show more than 50 KB free with no growth trend during the hardware run. Battery consumption is measured and reported but does not block experimental v0.1.

## Out of Scope

- TXT, Markdown, PDF, and XTC documents.
- X4, Sticky, X4 Pro, X4C, and other hardware acceptance.
- Multiple words per frame.
- Partial or windowed E-Ink refresh.
- Per-book RSVP settings.
- Language-specific ORP heuristics.
- A separate user-facing RSVP statistics screen.
- Touch-specific RSVP controls.
- Automatic entry into RSVP when a book opens.
- Upstream acceptance as a condition of release.

## Further Notes

- CrossInk-RSVP is prior art for activity lifecycle, transport controls, and refresh backpressure, but its unconditional reader replacement, byte-oriented tokenization, nominal timing, and coarse checkpoint are not copied.
- KOReader's RSVP plugin is prior art for pixel-metric ORP anchoring, not for CrossPoint lifecycle, persistence, or E-Ink scheduling.
- The repository already contains Russian localization, Unicode normalization and combining-mark utilities, glyph-advance measurement, visible-text progress mapping, a simulator, and host-side Google Test infrastructure.
- The specification and four implementation slices are published in the fork's GitHub Issues with the `ready-for-agent` triage label.
