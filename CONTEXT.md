# CrossRSVP Reading

CrossRSVP adds a focused rapid-reading experience to an e-reader while preserving a seamless path back to conventional page reading.

## Language

**Reading Mode**:
The currently active, mutually exclusive way in which a book is presented: Paged Mode or RSVP Mode.
_Avoid_: Reader type, view

**Paged Mode**:
Conventional reading in which a page of text remains visible until the reader advances it.
_Avoid_: Normal mode, ordinary mode

**Focus Reading**:
An optional fixation aid within Paged Mode that emphasizes eligible words using either Focus Prefix or ORP Emphasis. It is not a Reading Mode.
_Avoid_: Focus mode, third reading mode

**Focus Prefix**:
A Focus Reading strategy that emphasizes the opening portion of each eligible word.
_Avoid_: Bionic mode, ORP

**ORP Emphasis**:
A Focus Reading strategy that emphasizes one Optimal Recognition Point grapheme in each eligible word while leaving the rest of the word unchanged.
_Avoid_: ORP Mode, ORP Reading Mode

**RSVP Mode**:
Rapid Serial Visual Presentation in which one Active Word at a time is presented at a fixed visual focus, optionally surrounded by a Context Line.
_Avoid_: Speed mode, flash mode

**Active Word**:
The sole word in an RSVP frame whose ORP is emphasized and whose presentation advances Reading Position and Playback Pace.
_Avoid_: Main word, highlighted word, current token

**Context Line**:
Optional previous and following words displayed around the Active Word without advancing Reading Position or Playback Pace.
_Avoid_: Extra words, multi-word RSVP

**Context Boundary**:
A sentence, paragraph, chapter, or non-text transition across which a Context Line does not display neighboring words.
_Avoid_: Playback pause, page boundary

**Reading Position**:
The shared semantic location in a book from which either Reading Mode can continue.
_Avoid_: Page number, RSVP index

**Resume Anchor**:
The word used to reconcile Reading Position when switching modes. Leaving RSVP Mode anchors the last displayed word; after a page change in Paged Mode, the anchor becomes the first word on the displayed page.
_Avoid_: Bookmark, cursor

**Mode Switch**:
An explicit transition between Paged Mode and RSVP Mode that preserves the Resume Anchor while replacing the active presentation.
_Avoid_: View toggle, reader restart

**Document Element**:
A source-ordered unit of book content that may contain readable text or non-textual material requiring Paged Mode.
_Avoid_: Page block, render node

**RSVP Checkpoint**:
The durable per-book record from which RSVP Mode can restore its Resume Anchor after sleep, exit, or interruption.
_Avoid_: Bookmark, page progress

**Book Revision**:
The identity of the exact book content against which a Reading Position was recorded. A changed Book Revision makes its RSVP Checkpoint invalid.
_Avoid_: Filename, edition number

**Optimal Recognition Point (ORP)**:
The letter within the lexical core of the displayed word that remains aligned with a fixed visual focus.
_Avoid_: Word center, OVP

**Lexical Core**:
The letters and internal marks that determine a word's ORP after surrounding punctuation has been excluded.
_Avoid_: Trimmed word, clean word

**Playback Pace**:
The requested rate of word presentation, including the time required to update the display but excluding deliberate linguistic pauses. It cannot exceed the pace physically achievable by the panel.
_Avoid_: Timer delay, nominal WPM
