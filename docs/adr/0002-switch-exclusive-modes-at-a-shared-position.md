---
status: accepted
---

# Switch exclusive Reading Modes at a shared position

Paged Mode and RSVP Mode will be separate, mutually exclusive reading activities, with an in-reader Mode Switch that transfers a shared Reading Position between them. This avoids combining two rendering and input state machines while still letting an X3 reader immediately leave RSVP to reread context and then resume without losing their place.

The required X3 paths are a discoverable reader-menu action and an optional direct long-Confirm action. Entering RSVP is paused; short Confirm controls playback, short Back returns to Paged Mode, and long Back retains the existing Home/File Browser behavior. Leaving RSVP opens the page containing the last displayed word and temporarily highlights it. Returning without a page turn repeats that word; returning after a page turn begins at the first word of the displayed page.

## Consequences

- Reopening a book starts in Paged Mode; RSVP must be entered explicitly.
- Paged and RSVP persistence must reconcile through a semantic Resume Anchor rather than overwrite each other's native progress records.
- The RSVP renderer presents one word per frame, aligns a bold ORP letter between fixed guide marks, and targets 60–120 WPM on X3 pending hardware measurements.
- Russian-aware punctuation pauses are enabled by default: clause boundaries use `1.5×`, sentence boundaries `2×`, and paragraph boundaries `2.5×` the base interval.

