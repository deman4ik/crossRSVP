---
status: accepted
---

# Traverse Document Elements and checkpoint RSVP separately

RSVP Mode will traverse EPUB content in source order with a semantic cursor capable of identifying words and non-text Document Elements. It will maintain a versioned RSVP Checkpoint separate from CrossPoint's native page progress, then reconcile both through the Resume Anchor during a Mode Switch. A page-only cursor was rejected because pagination changes with layout and cannot reliably identify a word, while a flat word list would silently lose images, tables, and structural boundaries.

The checkpoint is written atomically every 30 seconds and on pause, Mode Switch, chapter transition, sleep, and exit. Page boundaries do not interrupt playback; chapter boundaries pause before their first word. Images, tables, and other unsupported non-text elements pause playback and offer a transition to Paged Mode rather than being silently skipped.

Words split only by layout or discretionary hyphenation are reconstructed. Internal hyphens remain in the Lexical Core, while surrounding punctuation and em dashes remain visible but do not affect ORP selection. Oversized words shrink to a configured minimum and otherwise pause with a Paged Mode fallback.

## Consequences

- RSVP settings are global in the MVP; only the checkpoint is per book.
- RSVP reading time contributes to existing book statistics, and native page progress advances when the Resume Anchor crosses page boundaries.
- The initial ORP heuristic selects letters 1, 2, 3, 4, and 5 for lexical lengths 1–2, 3–4, 5–6, 7–8, and 9 or more respectively.
