# RSVP Context Line

## Problem Statement

RSVP Mode currently isolates the Active Word, which maximizes fixation but removes the nearby linguistic context that helps some readers anticipate phrasing and recover meaning. A reader cannot optionally see the closest previous and following words while keeping the Active Word's ORP fixed, and adding ordinary lookahead naively could advance the document source, Reading Position, pacing, or checkpoint before the reader has actually reached those words.

## Solution

Add an optional RSVP Context Line that displays the nearest whole previous and following words around the Active Word on the same line. The Active Word remains the only word with ORP Emphasis and the only word that advances Reading Position, Playback Pace, navigation history, and the RSVP Checkpoint. A session-owned, fixed-capacity Context Window supplies visual context without allowing read-ahead to become logical progress. The global setting defaults to off and is available on every device supporting RSVP Mode.

## User Stories

1. As an RSVP reader, I want to enable a Context Line, so that I can retain nearby linguistic context while speed reading.
2. As an existing RSVP reader, I want the Context Line disabled after upgrading, so that the current single-word presentation does not change unexpectedly.
3. As an RSVP reader, I want the preference to apply globally, so that I do not need to configure every book separately.
4. As an RSVP reader, I want the Active Word to remain the sole paced word, so that future context is not counted as already read.
5. As an RSVP reader, I want the Active Word's ORP to remain at the fixed visual focus, so that context does not disturb my eye position.
6. As an RSVP reader, I want only the Active Word's ORP emphasized, so that the current reading target remains unambiguous.
7. As an RSVP reader, I want previous words shown to the left of the Active Word, so that I can recover the phrase I have just read.
8. As an RSVP reader, I want following words shown to the right of the Active Word, so that I can anticipate the phrase that follows.
9. As an RSVP reader, I want the nearest words to have priority, so that limited screen space preserves the most useful context.
10. As an RSVP reader, I want each side filled independently, so that a long word on one side does not waste usable space on the other.
11. As an RSVP reader, I want only complete context words displayed, so that context never ends in a clipped or ambiguous token.
12. As an RSVP reader, I want normal spacing preserved between displayed tokens, so that the Context Line reads naturally.
13. As an RSVP reader, I want standalone punctuation such as an em dash retained between context words, so that the displayed phrase preserves its meaning.
14. As an RSVP reader, I want context words rendered in the same font size as the Active Word, so that the line has one visual rhythm.
15. As an RSVP reader, I want context to be omitted rather than shrink the Active Word further, so that context never reduces the configured reading size.
16. As an RSVP reader, I want a long Active Word to retain the existing shrink and fallback behavior, so that enabling context does not change oversized-word handling.
17. As an RSVP reader, I want the Context Line to stop at a sentence boundary, so that the next sentence is not previewed before its pause.
18. As an RSVP reader, I want the Context Line to stop at a paragraph boundary, so that separate thoughts are not blended into one visual phrase.
19. As an RSVP reader, I want the Context Line to stop at a chapter boundary, so that chapter pauses remain meaningful.
20. As an RSVP reader, I want the Context Line to stop before images, tables, and other non-text Document Elements, so that unsupported content is never silently crossed.
21. As an RSVP reader entering or restoring a book, I want the first Active Word displayed immediately even when previous context is unavailable, so that backward reconstruction does not delay entry.
22. As an RSVP reader, I want partial context when lookahead is unavailable or storage is exhausted, so that the Active Word still renders reliably.
23. As an RSVP reader, I want an error discovered during read-ahead deferred until playback actually reaches it, so that valid current content is not stopped early.
24. As an RSVP reader, I want punctuation pauses and Playback Pace calculated only from the Active Word and its real following boundary, so that visual context does not alter timing.
25. As an RSVP reader, I want read-ahead excluded from the RSVP Checkpoint, so that sleep or exit resumes from what I actually read.
26. As an RSVP reader, I want the Context Line to remain unchanged while paused, so that the screen provides a stable phrase for review.
27. As an RSVP reader stepping forward, I want the Context Line recomputed around the new Active Word, so that manual navigation matches playback.
28. As an RSVP reader rewinding, I want previously read words to become following context around the rewound Active Word, so that navigation remains coherent.
29. As an RSVP reader, I want boundary, error, oversized-word, and end-of-book screens to remain uncluttered, so that status messages are not mixed with stale context.
30. As an RSVP reader, I want the feature to use the actual oriented screen width, so that portrait and landscape show as much context as each can fit.
31. As an Xteink user, I want the feature on every device that supports RSVP Mode, so that it is not tied to a particular panel model.
32. As an ESP32-C3 user, I want Context Line rendering without per-frame heap allocation, so that repeated playback does not fragment scarce RAM.
33. As an ESP32-C3 user, I want the nearest context preserved when a fixed buffer fills, so that memory safety takes precedence over distant words.
34. As a translated-UI user, I want the setting label localized, so that it follows the rest of the reader settings.
35. As a maintainer, I want read-ahead and logical progress tested separately, so that future refactoring cannot accidentally advance Reading Position or checkpoints.

## Implementation Decisions

- Extend RSVP Mode rather than introduce another Reading Mode. The feature is a visual aid named Context Line.
- Define the Active Word as the sole word whose ORP is emphasized and whose presentation advances Reading Position, Playback Pace, navigation history, statistics, and the RSVP Checkpoint.
- Add one global boolean RSVP Context Line setting. It defaults to off, persists with the existing RSVP settings, and uses translated user-facing text.
- Keep a fixed-capacity Context Window owned by RSVP Session. It contains the Active Word plus the nearest available previous and following display tokens with stable lifetime for the entire frame.
- Permit the source to read ahead into the Context Window, but do not treat physical source advancement as logical reading progress. Read-ahead must not mutate the current anchor, history cursor, checkpoint state, pacing deadline, or active-reading statistics.
- Do not let the renderer read or advance the document source. It receives an immutable Context Window and performs layout only.
- Preserve standalone non-lexical punctuation needed between displayed words. Such punctuation has no ORP and does not independently advance playback.
- Treat sentence ends, paragraph boundaries, chapter boundaries, and non-text Document Elements as Context Boundaries. Neither side of a Context Line crosses one.
- Keep the Active Word's ORP at the existing fixed focus coordinate. Fill the left and right regions independently with whole tokens, nearest first.
- Render context words in regular style at the Active Word's resolved font size. Only the Active Word receives the existing ORP Emphasis and optional guides.
- Resolve font size from the Active Word alone. Context never forces an additional size reduction; words that do not fit are omitted from the far edge.
- Preserve existing oversized Active Word behavior, including minimum-size fallback and the Paged Mode option.
- Use bounded, preallocated storage with no per-frame heap allocation. Store compact text spans and metadata rather than a maximum-sized document event for every context word.
- If storage cannot represent all otherwise fitting context, retain the Active Word and nearest neighbors and omit the farthest context. A valid Active Word never fails because visual context is incomplete.
- On initial entry or checkpoint restoration, allow the left side to remain empty until history accumulates naturally. Do not reopen and rescan the chapter solely to reconstruct prior context.
- If read-ahead encounters an error or future boundary, retain enough state to present it when playback reaches that position; render the current frame with whatever safe context is already available.
- Recompute the Context Line after automatic advance, manual step, and rewind. Keep it stable while paused.
- Show no Context Line on boundary, error, oversized-word, or finished screens because those frames have no Active Word.
- Use actual oriented viewable bounds and existing font metrics. Do not hardcode panel dimensions.
- The feature applies to every device where RSVP Mode is available; no device-specific preference is introduced.

## Testing Decisions

- Use the RSVP Session host seam as the primary behavioral test. Drive it with deterministic fake events and assert externally visible frames, anchors, deadlines, checkpoints, history navigation, and Context Windows rather than private container state.
- Extend the real EPUB fixture seam to verify that source read-ahead preserves document order, punctuation, and Context Boundaries without becoming logical progress.
- Add a pure Context Line layout seam that accepts measured token widths and oriented bounds. Test fixed ORP position, independent side filling, nearest-word priority, whole-word omission, punctuation, and overflow fallback without requiring a display.
- Verify that Active Word font selection is unchanged when Context Line is enabled and that context cannot trigger extra shrinking.
- Cover empty left context after entry and restoration, partial right context, end of book, future source errors, sentence/paragraph/chapter/non-text boundaries, pause, step, and rewind.
- Test the persisted setting through the existing settings seam: missing or false values remain off, true values enable the feature, and the translated label appears in English and Russian.
- Confirm boundary, error, oversized-word, and finished decisions do not expose a Context Window for rendering.
- Add capacity tests using short and maximum-length UTF-8 tokens. Assert deterministic nearest-first degradation and no failure of the Active Word.
- Run the RSVP host tests after the final code edit and build the default PlatformIO environment once.
- On hardware or the project simulator where applicable, verify all four orientations, a long Active Word, punctuation, Context Boundaries, pause/step/rewind, and repeated playback while monitoring free heap and the largest free block.

## Out of Scope

- Moving the Active Word or its ORP to center the complete phrase.
- Emphasizing ORPs or entire glyph runs in context words.
- Using a smaller, gray, dithered, inverted, or otherwise secondary context font.
- Clipping, ellipsizing, or breaking a context word to fill residual space.
- Crossing sentence, paragraph, chapter, or non-text Context Boundaries.
- Reconstructing words before the initial or restored anchor by rescanning the chapter.
- Counting previewed future words as read, checkpointed, or included in active-reading statistics.
- Changing RSVP pacing, punctuation-pause percentages, rewind distance, controls, ORP rules, or oversized-word fallback.
- Adding a per-book Context Line preference.
- Adding device-specific behavior or hardcoded screen dimensions.
- Performing partial-display refresh optimization as part of this feature.

## Further Notes

- The feature follows the domain language in CONTEXT.md and the session-ownership decision recorded in ADR 0005.
- The existing session already retains limited history and one lookahead event, but showing every word that fits requires a dedicated bounded Context Window rather than renderer-driven source access.
- Physical read-ahead and logical Reading Position are intentionally different concepts. This distinction is the central invariant for review and testing.
- The exact storage capacity must be justified against worst-case oriented width, UTF-8 representation, and the ESP32-C3 memory ceiling before implementation is merged.
