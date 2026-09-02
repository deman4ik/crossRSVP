# Paged ORP Emphasis

## Problem Statement

Readers who benefit from fixation guidance currently have two disconnected choices: Focus Reading in Paged Mode emphasizes a broad prefix of every eligible word, while RSVP Mode emphasizes one Optimal Recognition Point (ORP) grapheme. A reader cannot keep a complete page visible while using the same low-noise ORP cue that appears during RSVP playback. Replacing Focus Prefix would also disrupt readers who prefer its existing behavior.

## Solution

Extend Focus Reading within Paged Mode so the reader can choose Off, Focus Prefix, or ORP Emphasis. ORP Emphasis keeps the complete page visible and renders exactly one ORP grapheme in bold for each eligible word, using the same lexical and position rules as RSVP Mode. The choice is global, appears in both reader and text settings, updates the preview, applies to paged dictionary definitions, and transparently rebuilds layout-dependent caches when changed.

## User Stories

1. As a reader, I want ORP guidance on a complete page, so that I can use a precise fixation cue without RSVP playback.
2. As a reader, I want each eligible word to emphasize only its ORP grapheme, so that the page contains less visual noise than Focus Prefix.
3. As an RSVP reader, I want Paged Mode and RSVP Mode to select the same ORP grapheme, so that switching presentation does not move my fixation cue within a word.
4. As an existing Focus Reading user, I want Focus Prefix to remain available, so that an upgrade does not remove the reading aid I already use.
5. As a reader who does not want fixation guidance, I want to turn Focus Reading off, so that authored typography is shown without added emphasis.
6. As a reader, I want one clear Focus Reading choice among Off, Focus Prefix, and ORP Emphasis, so that two independent toggles cannot conflict.
7. As a reader, I want to change the Focus Reading strategy from the reader controls, so that I can compare strategies without leaving the book.
8. As a reader, I want to change the same strategy from text settings, so that all typography preferences are available together.
9. As a reader, I want the text preview to show the selected strategy immediately, so that I can understand the effect before returning to the book.
10. As a reader, I want the selected strategy to apply globally, so that books do not require repeated configuration.
11. As an existing user with Focus Reading disabled, I want an upgrade to preserve the Off state, so that the appearance of my books does not change unexpectedly.
12. As an existing user with Focus Reading enabled, I want an upgrade to preserve Focus Prefix, so that ORP Emphasis is adopted only when I choose it.
13. As a reader of Latin text, I want ORP selection to follow the established length heuristic, so that emphasis matches RSVP Mode.
14. As a reader of Cyrillic text, I want ORP selection to count Unicode characters rather than UTF-8 bytes, so that the correct grapheme is emphasized.
15. As a reader of decomposed Unicode text, I want combining marks to remain attached to their base grapheme, so that ORP emphasis does not split a visible character.
16. As a reader, I want surrounding punctuation to remain visible without affecting ORP position, so that quoted and punctuated words use the same fixation point as their lexical core.
17. As a reader, I want supported numeric tokens to follow the same ORP rules as RSVP Mode, so that emphasis is consistent across presentations.
18. As a reader, I want unsupported scripts or malformed tokens to remain readable without added emphasis, so that one token cannot prevent a page from rendering.
19. As a reader, I want authored bold text to remain fully bold, so that Focus Reading does not erase or reinterpret the publisher's emphasis.
20. As a reader, I want eligible regular text in paragraphs, headings, links, lists, quotations, and captions to use the selected strategy, so that fixation guidance is consistent across the page.
21. As a reader, I want paged dictionary definitions to use the selected strategy, so that looking up a word does not unexpectedly change the reading aid.
22. As a reader, I want ORP to be selected from the original word before hyphenation, so that a visual line break does not create a second fixation point.
23. As a reader, I want only the hyphenated fragment containing the original ORP grapheme to be emphasized, so that split words still have one fixation point.
24. As a reader, I want line and page breaks to account for the actual regular and bold glyph widths, so that ORP Emphasis never causes overlap or clipping.
25. As a reader, I want changing strategy to rebuild the affected section automatically, so that the displayed pagination is correct without manual cache maintenance.
26. As a reader, I want reopening a book to preserve both the selected strategy and my semantic Reading Position, so that cache rebuilding does not lose progress.
27. As a reader, I want the strategy to work in all four orientations, so that rotating the device does not alter the emphasis rules.
28. As an ESP32-C3 user, I want repeated strategy changes and page turns to remain stable within the device's memory ceiling, so that fixation guidance does not introduce crashes or progressive heap loss.
29. As a translator, I want all new labels and explanations to use the project's internationalization system, so that Focus Reading remains localizable.
30. As a future FreeInkBook integrator, I want stable ORP behavior vectors, so that a later engine port can match the active reader without copying accidental implementation details.

## Implementation Decisions

- Focus Reading remains a presentation aid within Paged Mode. It does not add a third Reading Mode or a new reader lifecycle.
- Replace the binary Focus Reading value with a compact three-valued strategy: Off, Focus Prefix, and ORP Emphasis. Existing persisted false and true values map to Off and Focus Prefix respectively.
- The strategy remains global. Per-book progress and Reading Position remain independent of the selected strategy.
- Preserve Focus Prefix behavior, including its current treatment of authored bold text.
- ORP Emphasis uses the established RSVP lexical core and position heuristic: lexical lengths 1–2, 3–4, 5–6, 7–8, and 9 or more select positions 1, 2, 3, 4, and 5 respectively.
- Extract or expose a shared, non-owning ORP selector so Paged Mode and RSVP Mode cannot drift. The selector returns an emphasized UTF-8 byte range or no range, performs no heap allocation, and uses only a small fixed amount of stack.
- Paged text is already normalized before emphasis selection. The shared selector must count lexical code points, keep combining marks with the selected base, ignore surrounding punctuation for position selection, and retain the established handling of internal punctuation.
- Unsupported scripts, invalid lexical content, or ranges that cannot be represented safely fall back to unchanged text. They do not fail page construction.
- Authored bold words remain fully bold. ORP Emphasis applies to eligible non-bold text across all page text roles and paged dictionary definitions.
- Select ORP from the original word before layout splitting or discretionary hyphenation. Carry the emphasized range through splitting so only the fragment containing that range renders it.
- Extend cached per-word emphasis metadata to represent a byte range and the measured positions needed for regular-prefix, bold-pivot, and regular-suffix rendering. Allocate this metadata only when the selected strategy requires it, and quantify the per-word and per-page increase before merging.
- Measure line layout with the same mixed styles used for rendering. Do not implement ORP Emphasis as a paint-only overlay.
- Treat the Focus Reading strategy as a layout input. Changing it invalidates the current section, performs normal repagination, and preserves the semantic Reading Position.
- Increment the section-cache format or layout generation so incompatible pages are discarded and rebuilt. Do not add a cache conversion path.
- Present one selector with Off, Prefix, and ORP values in reader controls and text settings. The text preview updates immediately. All user-facing labels and messages use translated string identifiers.
- Implement the feature only in the active EPUB pipeline. The unconnected FreeInkBook engine remains out of scope, while shared behavior vectors document the contract for a future port.
- Do not introduce per-word heap allocations, repeated allocation/free cycles in layout loops, or local buffers above the project's stack-safety limit.

## Testing Decisions

- Prefer the highest existing host seam that exercises parsed words, layout metadata, cache serialization, and rendered text spans together. Extend the current TextBlock round-trip coverage rather than testing private containers in isolation.
- Add focused tests around the shared ORP selector because it is the semantic contract used by both Reading Modes. Cover the complete length table, Latin, Cyrillic, digits, NFC/NFD equivalence, combining marks, surrounding and internal punctuation, malformed input, and unsupported scripts.
- Test Focus Reading as externally visible strategy behavior: Off adds no emphasis, Focus Prefix remains unchanged, and ORP Emphasis produces regular-prefix, one bold grapheme, and regular-suffix output.
- Test authored bold words and unsupported words as unchanged output rather than asserting internal flags.
- Test hyphenated words through the layout seam and assert that exactly one displayed fragment contains the original ORP emphasis.
- Extend cache round-trip tests to prove emphasis ranges and measured positions survive serialization. Verify that changing strategy rejects an older or differently keyed cache and rebuilds without changing Reading Position.
- Test settings migration at the persisted-value boundary: legacy false becomes Off, legacy true becomes Focus Prefix, and ORP is never selected implicitly.
- Exercise reader controls, text settings, preview, and dictionary layout at their existing component or activity seams. Assert translated labels and selected values, not panel indexes or private member layouts.
- Run the relevant host test suite once after the final code edit, then build the default PlatformIO environment once.
- On hardware, verify representative Latin and Cyrillic books in all four orientations. Repeatedly cycle strategies and pages while monitoring free heap and the largest free block; there must be no sustained loss attributable to the feature.

## Out of Scope

- A third Reading Mode, new reader Activity, or automatic playback in Paged Mode.
- Replacing or removing Focus Prefix.
- Per-book Focus Reading strategy.
- Alternative ORP cues such as inverse video, underlining, color, or grayscale highlighting.
- Applying ORP over authored bold text by removing the author's styling.
- Defining new ORP heuristics for CJK, RTL scripts, Greek, or other currently unsupported lexical classes.
- Converting old section caches in place.
- Integrating, modifying, or adding a runtime dependency on FreeInkBook.
- Changing RSVP pacing, controls, checkpoints, or Mode Switch behavior.

## Further Notes

- The feature follows the terminology and boundary recorded in ADR 0004: ORP Emphasis is a Focus Reading strategy within Paged Mode.
- The active page renderer currently stores enough Focus Prefix metadata for two text runs. ORP Emphasis requires three runs, so memory accounting and cache-format changes are mandatory parts of the implementation rather than optional optimization work.
- Manual device qualification remains necessary because host tests cannot validate E-Ink refresh behavior, font-cache pressure, or ESP32-C3 heap fragmentation.
