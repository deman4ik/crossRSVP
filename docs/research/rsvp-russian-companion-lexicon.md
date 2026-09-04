# Russian companion words for RSVP

Status: research recommendation  
Date: 2026-09-04  
Audience: RSVP design and firmware implementation

## Answer

The original list was too small, but a single large “stopword” list would be the wrong correction. The practical first implementation can cover **50 core Russian forms**, with **eight additional pronouns in the device-test layer**, without adding a morphological analyzer. The forms must be split by attachment behavior, while ambiguous, focus-bearing words remain standalone.

The recommended test build should contain:

1. a core structural layer enabled by `Short-word grouping`;
2. a deliberately broader experimental layer of eight nominative personal pronouns, compiled separately so it can be removed after device testing without rewriting the grouping algorithm;
3. an explicit deny-list for frequent but meaning-bearing or grammatically ambiguous forms.

This is broader than a three-letter heuristic while remaining compatible with the existing constraints: exact Unicode/casefold matching, at most three displayed words, one Active Word, no grouping across punctuation, and subjective validation on the device.

## Why word length or “function word” is not enough

Russian eye-tracking confirms that shorter and more frequent words are skipped more often and receive shorter fixations. It also reports that function-word status by itself does not reliably predict more skipping after frequency and length are controlled. In other words, `короткое` is useful evidence, but `служебное` is not a synonym for `можно ослабить`. [Zdorova et al., 2023](https://doi.org/10.3389/fpsyg.2023.1212701)

The RSVP evidence points in the same direction. Castelhano and Muter shortened only a small set of very frequent English words, but gave extra time to proposition-linking words such as *if*, *because*, *after*, and *although*. Their experiment concerned duration rather than multiword display, so it supports adaptive treatment but does not prove this grouping algorithm. [Castelhano & Muter, 2001](https://doi.org/10.1080/01449290110069400)

## Corpus check

The candidate forms were checked against the official Universal Dependencies Russian treebanks and the RNC frequency dictionary:

- a full local pass over SynTagRus counted 1,241,494 alphabetic or hyphenated tokens after filtering punctuation, numerals, comments, and technical CoNLL-U rows;
- an independent 282,367-token cross-check used SynTagRus, GSD, and Taiga;
- frequency was cross-checked against the RNC dictionary built from 92 million word usages across fiction, journalism, technical and business writing, and speech. [UD SynTagRus](https://universaldependencies.org/treebanks/ru_syntagrus/), [UD GSD](https://universaldependencies.org/treebanks/ru_gsd/), [UD Taiga](https://universaldependencies.org/treebanks/ru_taiga/), [Lyashevskaya & Sharov, 2009](https://philology.hse.ru/russian_dict)

In the three-treebank cross-check, the common prepositions below were 96.4–100% pure as prepositions by surface form and usually had their dependency head on the right. `же` attached left in 96.6% of examples and `ли` in 90.6%. `бы`, however, had a head on the left in only 68.2% of examples, so the earlier “always postfix” rule is not defensible.

## Recommended lexicon

### Core: attach forward

Primary and short high-confidence prepositions:

`в`, `во`, `на`, `с`, `со`, `к`, `ко`, `по`, `у`, `из`, `изо`, `от`, `ото`, `до`, `за`, `о`, `об`, `обо`, `для`, `при`, `без`, `под`, `подо`, `над`, `про`, `через`, `перед`, `передо`, `из-за`, `из-под`

Short derived prepositions with strong corpus directionality:

`между`, `после`, `около`, `среди`, `кроме`, `против`, `вместо`

Coordinating conjunctions and negation:

`и`, `а`, `но`, `или`, `либо`, `не`, `ни`

The preposition inventory starts from the closed grammatical list of primary Russian prepositions, then adds only frequent derived forms that remained directionally stable in the corpus. [Sichinava, 2018](https://rusgram.ru/pdf/sichinava-2018-prepositions.pdf)

`безо`, `надо`, and `чрез` are not in the practical exact-match list. `надо` is overwhelmingly a predicative/verb form in the corpus, `безо` produced more non-preposition than preposition observations after casefolding, and `чрез` did not occur in the analyzed SynTagRus snapshot. Their grammatical existence does not justify exact matching in firmware.

### Core: attach backward

`же`, `ж`, `ли`, `ль`

These are the reliable postpositive companions. They remain visible; “companion” means reduced visual prominence and shared frame, not deletion.

### Core: resolve on both sides

`бы`, `б`

These require a special local rule instead of membership in the backward table:

- `сделал бы` groups around `сделал`;
- `я бы сделал` may use `сделал` as Active Word and show `я` and `бы` as left companions when the experimental pronoun layer is active;
- the resolver may inspect only the two neighboring tokens in the same punctuation segment, preserving the three-word cap;
- if no non-companion anchor fits in that window, `бы/б` stays standalone rather than forming a companion-only frame.

This reflects both the corpus result and the grammar of `бы`: it is phonologically enclitic but its scope is the predication and its surface position varies. [RusGram: subjunctive](https://rusgram.ru/%D0%A1%D0%BE%D1%81%D0%BB%D0%B0%D0%B3%D0%B0%D1%82%D0%B5%D0%BB%D1%8C%D0%BD%D0%BE%D0%B5_%D0%BD%D0%B0%D0%BA%D0%BB%D0%BE%D0%BD%D0%B5%D0%BD%D0%B8%D0%B5)

### Experimental: nominative personal pronouns

`я`, `ты`, `он`, `она`, `оно`, `мы`, `вы`, `они`

These forms were pure pronouns in the corpus and had a head to the right in roughly 89–96% of examples. They are therefore reasonable device-test candidates for `я сделал` or `они пришли`, and they satisfy the product intent of including pronouns.

They are not safe enough for the structural core. Modern Russian does not have a stable lexical class of personal-pronoun clitics, and stress or attachment changes with order and focus. For example, `это сделал я` makes `я` contrastive and it should remain the Active Word. [Mleinek & Werkmann, 2006](https://www.isca-archive.org/speechprosody_2006/mleinek06_speechprosody.pdf), [Kibrik, 2004](https://www.philol.msu.ru/~otipl/new/main/people/kibrik-aa/files/Zero_anaphora_Slavic@DAARC_2004.pdf)

The test build can include this layer behind the same user-facing toggle because the toggle is off by default. The source should still keep the layer as a separate `constexpr` table so that device feedback can remove or narrow it without changing the resolver.

## Keep standalone in the first implementation

### Directionally ambiguous personal forms

`меня`, `тебя`, `его`, `её`, `ее`, `нас`, `вас`, `их`, `мне`, `тебе`, `ему`, `ей`, `нам`, `вам`, `им`, `себя`, `себе`

Their attachment direction varies with position. In the cross-corpus sample, `себя` and `себе` strongly preferred a head on the left, while `мне` more often pointed right and `меня` was almost evenly split. `его`, `её/ее`, and `их` are additionally ambiguous between personal-pronoun and possessive-determiner uses. An exact string table cannot resolve this reliably.

### Interrogative, relative, demonstrative, and quantified forms

Keep `что`, `как`, `кто`, `когда`, `чем`, `это`, `то`, `тот`, `та`, `те`, `такой`, `весь`, `всё`, `каждый` and their inflected or suffixed families standalone. They frequently carry the question, contrast, reference, or clause relation itself.

### Focus and discourse particles

Keep `да`, `только`, `даже`, `лишь`, `именно`, `ведь`, `вот`, `уже`, `ещё/еще`, `тоже`, `просто`, `уж` standalone. Many are frequent and directionally regular, but that does not make them semantically secondary.

### Subordinators

Keep `если`, `чтобы`, `чтоб`, `хотя`, `хоть`, `поскольку`, and `ибо` standalone under the already accepted `+35% per companion` timing model. They introduce or integrate propositions; grouping them while charging only 35% of a normal frame conflicts with the RSVP evidence that such words can require additional processing time. They can be reconsidered only together with a separate full-weight timing rule.

### Copular and auxiliary forms

Keep `есть`, `был`, `была`, `было`, `были`, `буду`, `будешь`, `будет`, `будем`, `будете`, `будут`, `будь`, and `будьте` standalone. Their surface forms can be auxiliary, copular, existential, or lexical; exact matching cannot distinguish the use.

## Resulting implementation contract

The first device build should therefore contain **50 core exact forms**:

- 44 forward forms: 37 prepositions, five coordinators, and `не/ни`;
- four backward forms: `же/ж/ли/ль`;
- two bidirectional forms: `бы/б`.

With the eight nominative personal pronouns, the test build evaluates **58 forms in total**. No runtime morphology, heap allocation, or language metadata is needed: the tables can be `static constexpr` flash data and the resolver only needs a bounded three-token lookaround.

The research supports the candidate boundaries and attachment tendencies. It does **not** prove that Russian readers will prefer the exact ORP grouping, smaller companion font, or `+35%` timing. Those remain product hypotheses to validate on the X3 device, as already agreed.
