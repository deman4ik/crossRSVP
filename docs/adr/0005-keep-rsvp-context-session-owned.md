---
status: superseded
superseded-by: 0006-replace-rsvp-context-line-with-presentation-groups.md
---

# Keep the RSVP Context Window session-owned

This decision is retained as historical context. ADR 0006 replaces the Context Line and its read-ahead window with bounded Presentation Groups.

RSVP Session will own a fixed-capacity Context Window containing the Active Word and the nearest available previous and following context. The source may be read ahead to populate this window, but an event becomes logically read only when it becomes the Active Word; visual context never advances Reading Position, Playback Pace, history navigation, or the RSVP Checkpoint. The renderer only lays out the supplied window and never advances the document source itself.

The Active Word keeps its ORP at the fixed visual focus and alone determines the line's font size. Whole context words fill the remaining left and right space independently, retain intervening standalone punctuation, and never cross a Context Boundary. Missing history, unavailable lookahead, or context-buffer exhaustion produces a partial Context Line rather than delaying or failing a valid Active Word.

## Consequences

- RSVP Context Line is a global, translated setting that defaults to off and applies on every device supporting RSVP Mode.
- The Context Window uses bounded storage and performs no per-frame heap allocation; nearest words have priority and the farthest context is omitted on overflow.
- Boundary, oversized-word, error, and end-of-book screens remain unchanged because they have no Active Word.
- Session tests must prove that read-ahead does not change logical progress, while layout tests cover fixed ORP alignment, whole-word fitting, punctuation, Context Boundaries, navigation, orientations, and overflow fallback.
