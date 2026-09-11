# Language-aware RSVP Presentation Groups with 7/5 limits

Status: implemented in v0.7.0; automated qualification and remaining device checks are recorded in [the validation report](../rsvp-v0.7.0-validation.md).

Implementation issue: [#14](https://github.com/deman4ik/crossRSVP/issues/14).

## Problem Statement

Russian RSVP grouping currently relies on exact forms without the agreed companion-length limits. It can show more peripheral text than the reader wants. English books do not have a corresponding Companion Eligibility policy, even though their articles, prepositions, pronouns, and apostrophe words are also candidates for shared frames.

The reader needs bounded context in the existing RSVP Mode, not the retired Context Line or a new Reading Mode. The language of the interface must not decide how a book is grouped. Missing, incorrect, multilingual, or unsupported EPUB language metadata must have predictable behavior and a per-book manual override.

## Solution

Extend Presentation Groups with language-aware Companion Eligibility. Both Russian and English use the same fixed limits: one Companion Word may contain up to seven letters; with two Companion Words, each may contain up to five letters. Special characters do not contribute to the letter count, and original spelling remains visible.

Preserve the existing Russian lexical inventory and attachment roles. Add 48 exact English forms and a general English rule admitting words with an internal straight or curly apostrophe between letters. English companions attach forward. This is a bounded lexical heuristic, not syntactic parsing or a promise that the Active Word is the semantic head of a phrase.

Provide a persisted per-book grouping-language selection: Auto, Russian, or English. Auto uses the primary EPUB language; an unknown or unsupported language produces standalone words. Keep the existing global grouping toggle, off by default, as the independent enablement control.

## User Stories

1. As an RSVP reader, I want the existing RSVP Mode to support bounded companions, so that I do not need a new Reading Mode.
2. As a Russian reader, I want the existing linguistic word selection preserved, so that arbitrary short words do not become companions.
3. As an English reader, I want articles grouped with the following Active Word, so that they do not always require separate frames.
4. As an English reader, I want common prepositions and possessive determiners included, so that grouping covers useful everyday constructions.
5. As an English reader, I want personal pronouns included in the first device trial, so that the feature is not limited to a tiny structural vocabulary.
6. As an English reader, I want and, or, but, and not included, so that short connecting words can share a frame.
7. As an English reader, I want eligible seven-letter forms included, so that the new seven-letter limit has practical coverage.
8. As a reader, I want a single companion limited to seven letters, so that long peripheral words do not dominate the frame.
9. As a reader, I want each of two companions limited to five letters, so that a three-word frame remains bounded.
10. As a reader, I want length limits applied only to companions, so that a long Active Word remains readable.
11. As a reader, I want letter counts independent of UTF-8 byte length and special characters, so that punctuation does not incorrectly reject short words.
12. As a reader, I want accented letters counted once, so that combining marks do not inflate word length.
13. As an English reader, I want short apostrophe words admitted without a contraction dictionary, so that both contractions and forms such as John's work consistently.
14. As an English reader, I want negative contractions eligible under the same limits, so that don't and wouldn't can share a frame.
15. As a reader, I want straight and curly internal apostrophes treated alike, so that typography does not change eligibility.
16. As a reader, I want surrounding quotation marks excluded from apostrophe-based eligibility, so that quoting a noun does not turn it into a companion.
17. As a reader, I want original punctuation and spelling preserved, so that grouping does not rewrite or expand text.
18. As a reader, I want no more than one Active Word and two companions, so that there is only one visual focus.
19. As a reader, I want companions unable to shrink the Active Word, so that context cannot make the primary reading target harder to see.
20. As a reader, I want rejected candidates displayed exactly once in source order, so that length and width limits never lose text.
21. As a reader, I want Grouping Boundaries preserved, so that a group cannot silently cross punctuation or document transitions.
22. As a reader with an English book and Russian menus, I want grouping selected from the book language, so that interface preferences do not affect reading rules.
23. As a reader, I want Auto to recognize regional English and Russian language tags, so that regional metadata works without manual configuration.
24. As a reader with missing language metadata, I want standalone words in Auto, so that the firmware does not guess a language from the alphabet.
25. As a reader with incorrect metadata, I want to select Russian or English for that book, so that I can override the metadata without editing the EPUB.
26. As a reader of several books, I want each manual selection persisted independently, so that one book does not change another book's grouping language.
27. As a reader of a multilingual book, I want one predictable policy for the whole book, so that grouping does not switch unexpectedly within a paragraph.
28. As a reader of an unsupported language, I want Auto to keep words standalone while retaining a manual override, so that unsupported metadata does not silently select English.
29. As a reader who disables grouping, I want existing single-word behavior preserved, so that language settings cannot enable the feature on their own.
30. As a reader, I want pause, step, rewind, and durable restoration to preserve complete acknowledged groups, so that navigation does not replay or skip companions.
31. As a button-only device user, I want the language selector reachable through existing reading controls, so that it does not depend on a touch-only shortcut.
32. As a reader changing orientation, I want layout and controls to remain correct, so that the feature works in all four orientations.
33. As an ESP32-C3 user, I want grouping to avoid per-frame heap allocation and repeated storage writes, so that longer reading sessions remain stable.
34. As a maintainer, I want core and experimental English lists separated under one toggle, so that device feedback can narrow a list without changing the reading model.
35. As the device tester, I want comparable on/off passages and explicit edge cases, so that subjective fixation comfort can determine acceptance without unsupported speed claims.

## Implementation Decisions

### Scope and existing contracts

- Extend the existing Companion Eligibility classifier, RSVP Session group resolver, reader settings integration, and EPUB metadata/persistence boundary. Reuse current rendering and navigation contracts.
- This specification intentionally extends the Russian-only, metadata-independent scope of ADR 0006 and the original Presentation Groups specification. Per-book language choice is now in scope; the grouping toggle remains global. Existing frame ownership, rendering, timing, and acknowledgement invariants remain unchanged.
- The retired arbitrary Context Line is not restored. A Presentation Group has exactly one Active Word and at most two Companion Words.
- Russian tables and forward/backward/bidirectional roles remain unchanged, subject to the new length filter and selected language policy. The apostrophe admission rule applies only to the English policy.

### Length and group resolution

- Apply inclusive limits of seven letters for one companion and five letters per companion when two are displayed. Do not apply these limits to the Active Word.
- Count letters, not bytes or all code points. Ignore special characters, including hyphens, apostrophes, and surrounding punctuation; treat a letter with combining diacritics as one letter. Keep the original token unchanged for rendering and position tracking.
- Length alone is not Companion Eligibility. Russian words use their existing lexical policy; English words require either an exact-form match or the internal-apostrophe rule.
- When two candidates fail the five-letter-per-word rule, retain at most one eligible candidate of up to seven letters. Preserve the existing priority: a postpositive candidate first, otherwise the nearest preceding candidate.
- Never jump over a rejected source word to assemble a group. A rejected prefix must be presented before the subsequent group, and a rejected suffix afterward. Every source word is displayed exactly once during forward playback.
- Retain the existing bounded lookahead and Active Word selection. If a valid group cannot be formed inside that window, fall back to standalone presentation; do not invent an all-companion frame.
- Determine membership and consume only the accepted, physically acknowledged group. Retain the existing handling of punctuation, paragraph/chapter transitions, non-text content, oversized tokens, errors, and end of book.

### English exact-form inventory

All 48 forms attach forward. Match exact lexical forms case-insensitively using the existing bounded normalization approach; preserve original case on screen. Surrounding punctuation may be excluded from lexical lookup, but must not erase a Grouping Boundary.

| Layer | Exact forms | Count |
| --- | --- | --- |
| Core articles | a, an, the | 3 |
| Core possessive determiners | my, your, our, its, their | 5 |
| Core prepositions and to | of, to, from, with, among, during | 6 |
| Experimental prepositions/particles | at, in, on, by, for, into, onto, upon, under, over, about, around, behind, beside, beyond, near, toward, within | 18 |
| Experimental seven-letter forms | without, against, between, towards, through | 5 |
| Experimental personal pronouns | I, you, he, she, it, we, they | 7 |
| Experimental coordinators | and, or, but | 3 |
| Experimental negation | not | 1 |

- Keep the 14 core and 34 experimental forms separately declared, but enable both together with the existing grouping toggle. Do not introduce a second experimental-mode toggle.
- The five seven-letter forms can only be single companions, never members of a two-companion pair.
- Words outside this inventory remain ineligible unless the English apostrophe rule applies. In particular, do not add bare auxiliaries, copulas, modals, ambiguous object/possessive forms, subordinators, or a general backward phrasal-verb list.
- Examples of excluded bare forms include is, are, have, can, his, her, me, them, this, that, if, because, before, after, only, and never. Ineligibility means retaining an Active Word role, not skipping the word or necessarily forbidding eligible companions beside it.
- Accept the heuristic limitation exemplified by he is ready: he IS / READY. Do not add a syntactic parser to force he is READY.

### General English apostrophe rule

- Admit any source word containing straight U+0027 or curly U+2019 apostrophe internally between letters, provided the word passes the same seven/five-letter limits. No contraction allowlist is required.
- The word attaches forward like other English companions. The rule deliberately includes negative contractions, possessives, names, and other apostrophe words; it does not attempt to prove a grammatical role.
- A word remains one source token and one companion slot. Do not expand contractions into hidden words, change spelling, or remove apostrophes for rendering.
- Leading or trailing quotation/apostrophe characters do not themselves grant eligibility. Quoted ordinary words can still qualify through the exact-form inventory.
- Do not erase apostrophes to create exact-form matches: its is not it's, and were is not we're. Recognizing either supported internal apostrophe is distinct from rewriting text.
- Other apostrophe-like characters are not added by this rule in the first implementation. Existing token handling remains intact.

### Language selection and persistence

- Provide a translated per-book selector with Auto, Russian, and English choices through the existing reading/settings UX. It must be usable on button-only devices as well as touch-capable devices.
- Persist the choice for the current book; an absent preference defaults to Auto. Manual selection overrides metadata, not the independent global grouping toggle or the book's actual content.
- In Auto, use the first EPUB language element in document order as the primary language. Recognize supported primary language subtags and their valid regional/script variants, including en, en-US, en-GB, ru, and ru-RU. Normalize tag case and surrounding metadata whitespace without guessing from token spelling.
- Do not concatenate multiple language elements, search later elements for a supported language, or infer English merely from Latin script.
- Missing, malformed, or unsupported primary language means standalone presentation in Auto. The manual Russian/English choices remain available.
- Select one policy for the whole book. Per-span language attributes, automatic language detection, and mixed-language switching are outside scope.
- Thread the resolved policy into RSVP Session when configuring reading. Changing a book's preference must not move Reading Position, consume unpresented words, or corrupt acknowledged group history; apply changes through the existing safe paused/settings flow.
- Preserve per-book selection across reopen/restart and ordinary derived-cache refresh. If metadata cache compatibility needs adjustment, rebuild derived metadata without deleting durable reading position or the manual choice.
- Guard redundant saves and use the project's existing SD persistence, storage abstraction, and throttling conventions. Do not persist a language selection on every frame.

### Rendering, pacing, and resource constraints

- Preserve fixed Active Word ORP, Active Word fit behavior, and the existing smaller regular companion font. Companions must not cause an additional Active Word size reduction.
- Keep whole-word width fallback and source-order-safe removal. Do not clip, ellipsize, or partly render a companion to satisfy the new limits.
- Preserve the existing companion timing contribution and punctuation pauses. Group navigation, physical acknowledgement, Resume Anchor, and RSVP Checkpoint remain group-based.
- With grouping disabled, language selection must not change single-word rendering, pacing, navigation, or restoration.
- Use bounded token inspection and compile-time lexical data. No per-frame heap allocation, expanding runtime dictionary, general-purpose NLP engine, or repeated normalized-string construction is allowed. Justify any added persistent state and its bounded memory cost under the project's ESP32-C3 constraints.
- Use existing translated UI strings, logical input, and orientation-aware rendering conventions. No generated files or local build configuration belong in the implementation commit.

## Testing Decisions

### Preferred seams and prior art

- Prefer observable behavior through the existing RSVP Session interface with a deterministic source: emitted Presentation Groups, Active Word index, source text, acknowledgement, history, and Resume Anchor. Reuse the current presentation-group, playback, and resume-lifecycle test patterns rather than adding an alternate grouping engine for tests.
- Add compact table-driven classification/letter-count tests only where the session seam would hide a boundary condition. Prior art is the existing companion-policy and lexical-core host tests. Test eligibility and displayed behavior, not the choice of helper functions or container layout.
- Use the existing EPUB-provider and metadata/persistence fixtures to verify language selection end to end, including existing caches, multiple language elements, preference round trips, and independent books. Extend these seams only as needed to expose the selected public behavior.
- Use the existing simulator reading/settings flow for the translated per-book selector, grouping on/off, manual override, rendered ORP/layout, and navigation. Prefer deterministic input and screenshots.

### Behavioral examples

In the following table, uppercase denotes the Active Word and a slash separates successive frames. Original spelling is preserved; uppercase I can therefore also appear as a companion. Examples assume grouping enabled, the appropriate language selected, no additional Grouping Boundary, and sufficient display width.

| Input | Expected frames or eligibility |
| --- | --- |
| with the house | with the HOUSE |
| in their house | in their HOUSE |
| during winter | during WINTER |
| during the night | DURING / the NIGHT |
| without help | without HELP |
| without a coat | WITHOUT / a COAT |
| he is ready | he IS / READY |
| I'm ready | I'm READY |
| we're not ready | we're not READY |
| I don't know | I don't KNOW |
| they're not ready | THEY'RE / not READY |
| wouldn't go | wouldn't GO |
| shouldn't go | SHOULDN'T / GO; eight letters exceed the single-companion limit |
| John's book | John's BOOK; five letters and an internal apostrophe |
| o'clock | Eligible by internal apostrophe and six letters when a following Active Word is available |
| 'house' and dogs' | Not eligible merely because of edge quotation/apostrophe characters |
| 'the' | Can qualify through the exact-form list, subject to existing boundaries |
| to. London | Separate frames; do not cross the sentence boundary |
| и после дождя | и после ДОЖДЯ |
| после дождя же | после ДОЖДЯ же |
| и против ветра | И / против ВЕТРА |
| против ветра же | ПРОТИВ / ВЕТРА же |
| не путешествовал | не ПУТЕШЕСТВОВАЛ; Active Word length is not limited by 7/5 |
| из-за дождя | из-за ДОЖДЯ; the companion has four letters |
| дом лес | ДОМ / ЛЕС; short length alone is not eligibility |

- Cover all 48 English forms, the unchanged Russian inventory, case variants, composed/combining accents, both supported apostrophes, and letter counts at five/six and seven/eight.
- Prove no skips, duplication, or reordering after length rejection, width rejection, unavailable lookahead, and boundaries. Test all-eligible runs as well as isolated words at end of book.
- Verify English metadata with Russian menus and Russian metadata with English menus; en-US/en-GB/ru-RU; missing, malformed, and unsupported tags; first-language precedence in multilingual metadata; and manual overrides for each case.
- Verify metadata language values are not concatenated and stale derived metadata cannot silently defeat primary-language selection. Reopening a book, restarting, and changing books must preserve the right manual preference and durable reading state.
- Verify group duration and pause rules remain unchanged, and manual forward, rewind, checkpointing, and Mode Switch reproduce whole acknowledged groups without replaying companions.
- After implementation, run the relevant host tests, target firmware build, sanctioned formatter, applicable static checks, and simulator flow. Do not claim device verification based on the simulator.
- On hardware, compare representative RU/EN passages with grouping off/on at the same requested pace. The user's subjective fixation comfort is the product criterion. Also verify all four orientations, long Active Words, font/width fallback, selector discoverability, and stable heap with more than 50 KB free under the project's device checklist.

## Out of Scope

- Firmware implementation, builds, flashing, release publication, or issue closure as part of writing this specification.
- Restoring static Context Line, a ticker, a new Reading Mode, more than three displayed words, multiple ORPs, or moving the fixed reading focus.
- Automatic language detection or switching per sentence, span, or token; using interface language as a book-language proxy.
- More languages, new Russian lexical forms, a general stopword list, runtime morphology, syntactic analysis, or automatic phrasal-verb recognition.
- A contraction dictionary, contraction expansion, admission based solely on surrounding quotes, or extra apostrophe-like characters beyond the two agreed variants.
- Additional threshold controls, separate experimental toggles, altered pacing formulas, new refresh strategies, or unrelated release/display work already present in the working tree.
- Redesigning Focus Reading or reopening the earlier Focus Reading discoverability task.
- Claims of proven comprehension/speed improvement, corpus coverage, or linguistic correctness for every heuristic grouping.

## Further Notes

- The user confirmed the complete product contract after the research and interview. This is one end-to-end implementation task with no required predecessor; do not split out a separate spec issue or duplicate parent ticket.
- Grammar-informed candidates and exact 7/5 thresholds are different kinds of evidence. RSVP timing research does not validate this particular grouping, font hierarchy, or letter budget. Device preference remains the agreed product test. [Castelhano and Muter, 2001](https://doi.org/10.1080/01449290110069400).
- Primary EPUB language is metadata about the publication, not automatic inheritance of language by every content span. The one-policy-per-book limitation is deliberate. [EPUB 3.3 language metadata](https://www.w3.org/TR/epub-33/#sec-opf-dclanguage).
- The working tree contains unrelated firmware/release work. Implementation must preserve it and verify against the actual branch state. This documentation-only commit does not bump firmware version; the completed user-visible implementation requires the project's one-time minor-version bump at the prescribed release stage.

## What to build

Deliver language-aware Presentation Groups end to end: persisted per-book language selection, correct EPUB primary-language handling, 7/5 limits for Russian and English, the 48-form English policy plus internal-apostrophe admission, and unchanged rendering/progress guarantees. The result must be demonstrable in the simulator and ready for the agreed subjective device check.

## Acceptance criteria

- [ ] Russian and English enforce one companion of at most seven letters or two of at most five letters each; Active Word length is not restricted by these limits.
- [ ] Special characters do not count as letters; accented letters count once; source spelling, punctuation, and token identity remain unchanged.
- [ ] All 48 English exact forms have forward eligibility, with 14 core and 34 experimental forms declared separately but enabled together.
- [ ] Any word with internal U+0027 or U+2019 between letters can qualify under 7/5 without a contraction list, including negative contractions and John's.
- [ ] Edge quotes in 'house' and the trailing mark in dogs' do not grant apostrophe eligibility; ordinary lexical lookup still works for quoted eligible forms.
- [ ] Russian lexical forms and attachment roles remain unchanged; unsupported bare English forms do not become companions merely because they are short.
- [ ] Every displayed group has exactly one Active Word and at most two companions, preserves boundaries/order, and never consumes rejected candidates or skips source words.
- [ ] The specified RU/EN and apostrophe examples pass, including the seven-letter single-companion and five-letter pair boundaries.
- [ ] The translated Auto/Russian/English selector is reachable through existing reading controls on button-only and touch-capable devices, persists per book, and defaults to Auto when absent.
- [ ] Auto uses the primary EPUB language independently of menu language; supported tag variants select the right policy, and unknown/invalid/unsupported primary languages produce standalone words.
- [ ] Multiple EPUB language values are not concatenated or searched for a later supported language; manual override applies to the whole book without per-span detection.
- [ ] Manual choices survive reopen/restart and ordinary derived-cache refresh, do not leak between books, and do not enable a disabled global grouping toggle.
- [ ] Changing the language choice preserves Reading Position and acknowledged history; metadata/cache compatibility is handled without deleting durable reader state.
- [ ] Active Word ORP and fit behavior remain unchanged; companions never cause extra shrinking or partial-word rendering, and width fallback preserves source order.
- [ ] Group pacing, pause classes, navigation, acknowledgement, Resume Anchor, checkpoints, and restoration retain existing semantics with no duplicate companions.
- [ ] No per-frame heap allocation or repeated per-frame SD preference writes are introduced; new persistent/bounded state follows project resource and storage rules.
- [ ] Relevant host tests, firmware build, formatting/static checks, and affected simulator scenarios pass; the device-only checklist and subjective RU/EN comparison results are recorded before the firmware is declared fully verified.

## Blocked by

None (can start immediately).
