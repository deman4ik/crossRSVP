# RSVP Presentation Groups

## Problem

The current optional Context Line renders arbitrary neighboring tokens around the Active Word. It can expose up to three tokens on each side, reads ahead until that visual capacity or a boundary is reached, and draws context at the same resolved size as the Active Word. On the X3 this produces enough equally prominent peripheral text to compete with the fixed ORP, making even the Active Word harder to read.

The useful part of the idea is narrower: very short structural words are often easier to understand beside a nearby lexical word than as isolated RSVP frames. A length-only rule is unsafe in Russian, however, because short pronouns and particles can carry contrast or change attachment direction. The product therefore needs bounded linguistic grouping rather than a smaller version of the same Context Line.

Focus Reading has a separate discoverability defect. It is registered as a Reader setting but classified as nested Text Settings, so the flat Reader list removes it. The nested Style row works, while the direct toolbar toggle is available only on touch devices. The X3 therefore has no direct Reader-list toggle even though the current guide still documents one.

## Solution

Replace Context Line with optional Short-word Grouping inside the existing RSVP Mode. Each frame presents exactly one Presentation Group containing one Active Word and zero, one, or two Companion Words. The Active Word keeps the fixed ORP and configured RSVP sizing behavior. Companion Words provide only limited structural context and do not receive their own ORP.

Use a fixed Russian exact-form policy with forward, backward, and bidirectional roles. The structural core contains 50 forms; eight nominative personal pronouns form a separately declared experimental layer included in the first device build. The grouping toggle defaults to off. No EPUB language metadata or runtime morphology is required.

Restore Focus Reading as a direct row in the flat Reader settings while preserving the existing Text Settings > Style row.

## User Stories

1. As an RSVP reader, I want short structural words displayed with a nearby lexical word, so that they are not isolated into visually weak frames.
2. As an RSVP reader, I want grouping to extend the existing RSVP Mode, so that I do not have to learn or select a third Reading Mode.
3. As an existing reader, I want Short-word Grouping off after upgrading, so that RSVP behavior does not change without consent.
4. As a reader who previously enabled Context Line, I want the new feature to remain off until I choose it, so that a legacy value cannot silently select different behavior.
5. As a reader, I want no more than three words in a frame, so that grouping cannot recreate the crowded Context Line.
6. As a reader, I want every frame to contain exactly one Active Word, so that the current reading target is unambiguous.
7. As a reader, I want the Active Word ORP to remain at the fixed visual focus, so that grouping does not move my fixation point.
8. As a reader, I want only the Active Word to receive ORP Emphasis, so that Companion Words do not compete for attention.
9. As a reader, I want Companion Words visibly secondary but fully readable, so that they provide context without becoming noise.
10. As a reader, I want the configured Active Word size preserved, so that companions cannot make the primary word harder to read.
11. As a reader, I want an oversized Active Word to keep its existing fallback behavior, so that grouping never makes an otherwise readable word fail.
12. As a reader, I want an overflowing companion omitted as a whole word, so that I never see clipped or partial context.
13. As a reader, I want rejected companions presented separately in source order, so that grouping never skips or reorders text.
14. As a reader, I want Russian prepositions attached to the word that follows, so that phrases such as `в доме` appear naturally.
15. As a reader, I want postpositive `же/ж/ли/ль` attached to the preceding Active Word, so that their normal relationship remains visible.
16. As a reader, I want `бы/б` resolved from its local context, so that both `сделал бы` and `я бы сделал` produce coherent frames.
17. As a reader, I want common nominative personal pronouns included in the device trial, so that the grouping experiment is not limited to a tiny preposition list.
18. As a reader, I want ambiguous and focus-bearing words kept standalone, so that short spelling alone cannot suppress an important word.
19. As a reader, I want `если`, `чтобы`, and similar proposition-linking words kept standalone under the current timing model, so that they retain enough processing time.
20. As a reader, I want matching to work with uppercase and lowercase Russian text, so that sentence position does not change grouping.
21. As a reader, I want grouping to work without trustworthy EPUB language metadata, so that ordinary Russian books behave consistently.
22. As a reader, I want commas, semicolons, colons, and dashes to stop grouping, so that distinct phrase segments are not merged.
23. As a reader, I want sentence, paragraph, and chapter boundaries to stop grouping, so that presentation preserves linguistic pauses.
24. As a reader, I want images, tables, and other non-text elements to stop grouping, so that unsupported content is not crossed silently.
25. As a reader, I want punctuation attached to the last displayed word preserved, so that meaning and pacing remain intact.
26. As a reader, I want a group with one companion to stay longer than an Active Word alone, so that the added word has real processing time.
27. As a reader, I want a three-word group to account for both companions, so that nominal WPM does not accelerate unexpectedly.
28. As a reader, I want manual forward to advance to the next displayed group, so that a button press matches what I see.
29. As a reader, I want rewind to restore the exact earlier group, so that navigation does not regroup the same text differently.
30. As a reader, I want pausing to keep the current Presentation Group stable, so that I can inspect it without visual changes.
31. As a reader, I want a physically acknowledged group committed atomically, so that progress cannot stop in the middle of a displayed frame.
32. As a reader leaving RSVP, I want Reading Position anchored after the whole acknowledged group, so that Paged Mode does not repeat a companion.
33. As a reader restoring RSVP, I want continuation after the whole saved group, so that a companion does not return as a new Active Word.
34. As an X3 user, I want Focus Reading visible directly under Reader settings, so that I can find the feature without discovering a hidden Style tab.
35. As a reader accustomed to Text Settings, I want Focus Reading to remain under Style as well, so that the existing path continues to work.
36. As a maintainer, I want the experimental pronouns isolated from the structural core, so that device feedback can narrow them without redesigning grouping.
37. As an ESP32-C3 user, I want grouping to avoid per-frame heap allocation, so that repeated playback cannot fragment scarce RAM.
38. As a maintainer, I want group behavior verified through presented frames, progress, and navigation, so that tests remain stable across internal refactoring.

## Implementation Decisions

### Presentation Contract

- A Presentation Group contains exactly one Active Word and at most two Companion Words.
- Only the Active Word receives ORP Emphasis and aligns its ORP with the existing fixed focus coordinate.
- Resolve the Active Word's point size using the existing fit loop before measuring companions.
- Render companions in regular style at the next smaller point size supported by the selected font family. If no smaller supported size exists, use the smallest supported size; never synthesize an unsupported point size.
- Preserve ordinary spaces between displayed words. Do not clip, ellipsize, hyphenate, or partly draw a companion.
- Add companions nearest-first. If the full group does not fit, remove the farthest companion and remeasure. If one companion still does not fit, show the Active Word alone.
- A companion never causes an additional Active Word size reduction and never changes the Active Word ORP.
- A valid Active Word must remain renderable even if every companion is rejected.

### Companion Eligibility

Match the complete lexical core by Unicode code point after Russian case folding. Preserve `ё` as distinct from `е`; the accepted table explicitly contains spelling variants where necessary. Surrounding punctuation is not part of the lookup. Do not depend on EPUB language metadata.

Reuse or extract the existing allocation-free Cyrillic lowercase behavior; do not allocate a normalized string per token.

#### Core forward forms

Primary and high-confidence prepositions:

`в`, `во`, `на`, `с`, `со`, `к`, `ко`, `по`, `у`, `из`, `изо`, `от`, `ото`, `до`, `за`, `о`, `об`, `обо`, `для`, `при`, `без`, `под`, `подо`, `над`, `про`, `через`, `перед`, `передо`, `из-за`, `из-под`, `между`, `после`, `около`, `среди`, `кроме`, `против`, `вместо`

Coordinators and negation:

`и`, `а`, `но`, `или`, `либо`, `не`, `ни`

#### Core backward forms

`же`, `ж`, `ли`, `ль`

#### Core bidirectional forms

`бы`, `б`

`бы/б` must not be implemented as ordinary backward-only entries. The resolver examines only the adjacent two-token window within the same Grouping Boundary:

- `сделал бы` anchors on `сделал`;
- `я бы сделал` may anchor on `сделал` when `я` is eligible in the experimental layer;
- if no Active Word can be retained in the bounded window, `бы/б` remains standalone.

#### Experimental forward forms

`я`, `ты`, `он`, `она`, `оно`, `мы`, `вы`, `они`

Keep these in a distinct `constexpr` table even though the first device build enables them together with the core. Their attachment is directionally common but not linguistically unconditional; subjective device testing may narrow the table without changing the setting or group resolver.

#### Standalone forms

- Directionally ambiguous personal forms: `меня`, `тебя`, `его`, `её`, `ее`, `нас`, `вас`, `их`, `мне`, `тебе`, `ему`, `ей`, `нам`, `вам`, `им`, `себя`, `себе`.
- Interrogative, relative, demonstrative, and quantified families including `что`, `как`, `кто`, `когда`, `чем`, `это`, `то`, `тот`, `весь`, and `каждый`.
- Focus and discourse particles including `да`, `только`, `даже`, `лишь`, `именно`, `ведь`, `вот`, `уже`, `ещё`, `еще`, `тоже`, `просто`, and `уж`.
- Clause integrators `если`, `чтобы`, `чтоб`, `хотя`, `хоть`, `поскольку`, and `ибо`. Under the accepted 35% companion contribution they would receive less time despite evidence that proposition-linking words may require extra processing.
- Copular and auxiliary forms of `быть`, which cannot be disambiguated by an exact surface-form table.

The companion-lexicon research record contains the evidence and corpus statistics behind these boundaries.

### Group Construction

- Never cross a comma, semicolon, colon, dash, sentence terminator, paragraph boundary, chapter boundary, non-text Document Element, oversized-word event, source error, or end of book.
- Punctuation attached to the Active Word remains visible and continues to drive its existing pause class.
- Prefix companions attach to the following Active Word; backward companions attach to the preceding Active Word; `бы/б` use the bounded bidirectional rule above.
- When more than two eligible companions compete, retain a postfix companion first, then the nearest preceding companion. Preserve source order in the rendered frame.
- Example overflow: `я не сделал бы` becomes standalone `я`, then the group `не сделал бы`; no source word appears in both frames.
- Never emit a frame containing only Companion Words.
- Group resolution consumes every source word included in the group atomically.
- Resolve membership before presenting any buffered token. A prefix companion rejected by capacity or width is emitted as an earlier standalone group; a rejected postfix companion remains for a later group. Neither case may reorder the source text around the Active Word.

### Pace, Progress, and Navigation

- Group duration is `baseInterval(active) + 35% * baseInterval` for each displayed companion, followed by existing punctuation and structural pauses.
- A Playback Step advances one Presentation Group, not one source word.
- Manual forward and rewind operate on Presentation Group history. They must reproduce the exact group membership and source order previously presented.
- Physical presentation acknowledgement commits the entire group.
- The Resume Anchor and RSVP Checkpoint advance to the last source word in the acknowledged group.
- Entering Paged Mode and normal RSVP restoration continue after the complete acknowledged group. They do not replay one of its companions as a new Active Word.
- A candidate rejected from a group is not consumed with that group. It is presented separately in original source order, before or after the group according to its original position.

### Settings and Migration

- Replace the user-facing `Context Line` toggle with `Short-word grouping` in the RSVP settings.
- The new setting is global, translated, and off by default.
- Do not interpret or migrate the old Context Line saved value. Missing new value and any legacy-only file both resolve to off.
- Stop writing the old Context Line key after the new setting is saved; tolerating an unused legacy key during load is acceptable.
- Add Focus Reading directly to the flat Reader category while retaining its existing row under Text Settings > Style.
- Both Focus Reading rows edit the same setting and show the same current value.
- Update the Focus Reading guide to document both paths accurately.

### Memory and Ownership

- Keep group construction in RSVP Session; the renderer receives an immutable, already resolved Presentation Group and does not advance the source.
- Replace the existing six-neighbor Context Window with fixed storage for the three displayed source words plus one bounded unconsumed lookahead event needed to resolve overflow without reordering.
- Store the 50 core forms and eight experimental forms as compile-time data in flash. Do not build a heap container or copy the tables into DRAM.
- Perform no per-frame heap allocation. Reuse bounded token buffers and font-measurement scratch state across frames.
- Any new group-history metadata must have a documented fixed capacity and byte cost before implementation. Reuse the existing rewind/history capacity rather than maintaining parallel word and group histories.

## Testing

- Add pure host tests for casefolded exact matching, punctuation stripping, forward/backward/bidirectional roles, deny-list examples, and every accepted form.
- Add table-count assertions for 50 core and eight experimental entries so accidental additions require an explicit test change.
- Cover `сделал бы`, `я бы сделал`, `не сделал бы`, `я не сделал бы`, `а он пришёл`, `что произошло`, and `это сделал я`.
- Cover every Grouping Boundary and prove that no group crosses it.
- Add layout tests for zero, one, and two companions; smaller supported font selection; fixed Active ORP; farthest-first removal; and Active-only fallback.
- Prove that a rejected companion is not consumed and that all displayed companions are consumed exactly once.
- Prove that a rejected prefix is presented before the Active Word and a rejected postfix after it.
- Test automatic advance, manual forward, rewind, pause, physical presentation acknowledgement, checkpoint creation, Mode Switch, and durable restoration in Presentation Group units.
- Test setting migration: an old Context Line value does not enable Short-word Grouping, and the new value persists independently.
- Test that Focus Reading appears in both Reader and Text Settings > Style and that either row changes the same persisted value.
- Run the RSVP host suite and default PlatformIO build after implementation.
- On X3 hardware, compare grouping off/on in representative prose at the same WPM. Subjective fixation comfort is the acceptance criterion; also verify all four orientations, long words, overflow examples, pause/step/rewind, and Focus Reading discoverability.
- During repeated playback and settings switching, monitor free heap and largest free block and confirm that neither trends downward.

## Acceptance Criteria

- [ ] RSVP settings expose the translated `Short-word grouping` toggle; it is global and defaults to off.
- [ ] A saved legacy Context Line value does not enable Short-word Grouping, and saving the new setting no longer writes the legacy key.
- [ ] With grouping off, RSVP presentation, timing, navigation, progress, and restoration retain their current behavior.
- [ ] With grouping on, every displayed frame has exactly one Active Word and no more than two Companion Words.
- [ ] The Active Word keeps the fixed ORP and existing fit behavior; companions use regular style at one supported font step smaller and cannot force further Active Word shrinking.
- [ ] A companion that does not fit is omitted intact and later displayed exactly once in original source order.
- [ ] Exact casefolded matching recognizes all 50 core forms and eight separately declared experimental pronouns without relying on EPUB language metadata or per-token heap allocation.
- [ ] Forward, backward, and bidirectional attachment reproduce the specified examples, including `сделал бы`, `я бы сделал`, and `я не сделал бы`.
- [ ] Ambiguous pronouns, focus particles, clause integrators, and auxiliary forms listed as standalone never become companions.
- [ ] No group crosses punctuation, sentence, paragraph, chapter, non-text, oversized-word, error, or end-of-book boundaries.
- [ ] Display duration adds 35% of the Active Word base interval for each displayed companion before existing pause adjustments.
- [ ] Automatic advance, manual forward, rewind, and pause operate on stable Presentation Groups.
- [ ] Physical acknowledgement, checkpoints, Mode Switch, and durable restoration commit through the last source word of the whole acknowledged group without replaying a companion.
- [ ] Focus Reading is visible both directly in Reader settings and under Text Settings > Style; both rows edit the same persisted value, and the guide documents both paths.
- [ ] Host tests cover lexical policy, group construction, layout, timing, navigation, progress, restoration, settings migration, and Focus Reading discoverability.
- [ ] The default firmware build succeeds, and implementation performs no per-frame heap allocation or progressive loss of free heap/largest free block during repeated playback.
- [ ] On X3 hardware, grouping is subjectively no harder to fixate on than single-word RSVP at the same WPM and passes the specified orientation, overflow, navigation, and settings checks.

## Blocked By

None. The task can start immediately.

## Out of Scope

- A static Context Line, ticker, or smoothly scrolling text.
- More than three displayed words.
- Multiple emphasized ORPs or moving the fixed Active ORP.
- Runtime POS tagging, syntactic parsing, language detection, or a general multilingual stopword system.
- Grouping ambiguous oblique pronouns, proposition-linking subordinators, copular forms, or focus particles in the first implementation.
- A second user-facing switch for the experimental pronoun table.
- Per-book grouping preferences.
- Partial-refresh optimization or a new Reading Mode.

## Further Notes

- ADR 0006 records why bounded Presentation Groups replace the previous session-owned Context Line.
- The implementation should delete or rename Context Line-specific types and tests rather than preserve both abstractions behind the new setting.
- This is a user-visible capability change and therefore requires one minor firmware version bump after implementation and verification, before the final release build.
