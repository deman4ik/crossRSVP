---
status: accepted
---

# Replace RSVP Context Line with Presentation Groups

CrossRSVP will retire the static RSVP Context Line and present one bounded Presentation Group per frame. A Presentation Group contains exactly one Active Word and at most two eligible Companion Words. Only the Active Word receives ORP Emphasis; companions remain visible in regular style at one supported font-size step below the resolved Active Word size.

Companion Eligibility is determined by a bounded, Russian-specific lexical policy rather than by displaying arbitrary previous and following words. Forward, backward, and bidirectional attachment roles allow short prepositions, coordinators, negation, selected clitic particles, and an experimental set of nominative personal pronouns to share a frame with an adjacent Active Word. Grouping never crosses a punctuation pause or document boundary.

The global `Short-word grouping` setting defaults to off and replaces the old Context Line setting without migrating its saved value. A Playback Step, history entry, pacing deadline, and physical presentation acknowledgement operate on the complete Presentation Group. The Resume Anchor and durable checkpoint use the last source word in the most recently acknowledged group; normal restoration continues after that anchor so no displayed companion is repeated.

## Consequences

- Static left/right context read-ahead and its six-neighbor capacity are removed; RSVP Session builds only the current group with fixed-capacity state and no per-frame heap allocation.
- The Active Word is sized first. Companions never shrink it further; an overflowing group drops the farthest companion and ultimately falls back to the Active Word alone.
- Playback duration is one Active Word interval plus 35% for each displayed companion, before existing punctuation and boundary pauses are applied.
- Manual forward, rewind, and history navigation operate in Presentation Groups rather than source-token increments.
- The Russian exact-form tables remain separate by role and confidence so that the experimental pronoun layer can be narrowed after subjective X3 testing without changing the resolver.
- Clause-integrating subordinators, ambiguous pronouns, copular forms, and focus-bearing particles remain standalone in the first implementation.
- Focus Reading remains available in nested Text Settings and also returns as a direct Reader setting because button-only devices cannot use the touch-only toolbar shortcut.
