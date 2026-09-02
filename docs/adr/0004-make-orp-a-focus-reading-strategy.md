---
status: accepted
---

# Make ORP a Focus Reading strategy

CrossRSVP will add ORP Emphasis as a strategy of Focus Reading within Paged Mode, not as a third Reading Mode. The global Focus Reading setting becomes one choice among Off, Focus Prefix, and ORP Emphasis; persisted false and true values migrate to Off and Focus Prefix respectively, so ORP is always an explicit choice. This avoids a second toggle and keeps page navigation, reading position, and lifecycle ownership in the existing paged reader.

ORP Emphasis uses the same lexical core and position heuristic as RSVP Mode and emphasizes exactly one ORP grapheme in each eligible word. It is computed from the original word before layout or discretionary hyphenation, and only the displayed fragment containing that grapheme receives emphasis. Eligible non-bold text is treated consistently across body text, headings, links, lists, captions, text preview, and paged dictionary definitions; authored bold text and unsupported lexical content remain unchanged.

## Consequences

- Switching Focus Reading strategy performs a correct repagination because mixed regular and bold metrics can change line and page breaks.
- The section-cache format and generation key must distinguish all three strategies; incompatible cached sections are discarded and rebuilt rather than converted.
- The initial implementation changes only the active `lib/Epub` pipeline. The unconnected FreeInkBook implementation remains unchanged until it becomes a runtime dependency, but future ports must use the same ORP test vectors.
- Acceptance covers Latin, Cyrillic, digits, NFC/NFD text, punctuation, hyphenated fragments, settings migration, cache reopen, text preview, dictionary pages, PlatformIO build, repeated-switch heap stability, and manual checks in all four orientations.
