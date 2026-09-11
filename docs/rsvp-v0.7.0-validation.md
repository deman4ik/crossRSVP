# CrossRSVP v0.7.0 — validation for #13 and #14

The implementation adds a bilingual Pages site and per-book Russian/English RSVP
grouping. It is ready for device evaluation; it is **not hardware-qualified**.
The existing v0.6.0 GitHub prerelease is unchanged and is excluded from stable
site downloads. This task does not publish a new firmware release.

## Automated evidence

- Host suite: **365/365 PASS**, including parallel CTest execution. Coverage includes
  all 48 English forms, apostrophes and accented letters, 7/5 rejection, source
  order/active indices, width fallback, paused reconfiguration/history, preference
  round trips and isolation, backup recovery, invalid records, cache preservation,
  and real streamed EPUB primary-language parsing.
- Native simulators: X3, X4 and X4 Pro builds pass. **27 language/orientation
  scenarios PASS**: four language/global-toggle cases and four orientations per
  model, button selection on X3/X4, touch selection on X4 Pro. Selector scenarios
  restart the process after rebuilding derived caches while preserving durable
  files. The X4 Pro scenario verifies the unchanged paused frame across Settings
  and English grouping on the next step. An additional touch pace-setting
  regression also passes (**28 simulator scenarios in total**).
- `pio check -c platformio.qualification.local.ini -e rsvp_x3_release
  --fail-on-defect low --fail-on-defect medium --fail-on-defect high`: **PASS,
  no defects**. A separate exhaustive cppcheck pass over the changed RSVP,
  preference, parser, selector, Settings and cache implementation also passes.
- The final CI follow-up also passes the strict `default` cppcheck profile.
  Launch-context parameters use const references; the EPUB constructor parameter
  no longer shadows its field, avoiding a false moved-variable diagnostic.
  The host suite was rerun (365/365 PASS), followed by all three release builds.
- `./bin/clang-format-fix -g` and `git diff --check`: PASS.
- Site: **11/11 generator scenarios PASS**; Chromium browser checks PASS for
  language detection/selection/storage failures, full RU/EN switching, word
  playback, fixed ORP, pause/continue, pace, finish/restart, language/visibility
  pause, keyboard focus, reduced motion, 320/390/1440px layouts, and static
  downloads without JavaScript. Fixture and live GitHub API builds pass.
- Desktop/mobile site screenshots and firmware selector/group screenshots were
  inspected. The site was visually compared with crosspointreader.com; the
  illustration and copy are original, with the upstream project credited.

Local evidence is under ignored `artifacts/issues-13-14/`. Simulator binaries
embed the pre-bump 0.6.0 version because behavior was verified before the required
one-time version increment; the final firmware packages embed **0.7.0**. The
simulator injects heap values and does not model real E-Ink refresh timings.

## Reproduction

```sh
cmake -S test -B build/test
cmake --build build/test -j 6
ctest --test-dir build/test --output-on-failure -j 6
pio run -e simulator_x3 -e simulator_x4 -e simulator_x4pro
python3 scripts/qualify_rsvp_simulator.py --device x3 --scenario language-selector-buttons --scenario language-auto-english
python3 scripts/qualify_rsvp_simulator.py --device x4pro --scenario language-selector-touch --scenario language-auto-english
cd site
npm ci
npm test
npx playwright install chromium
npm run test:browser
npm run build
```

For isolated macOS qualification, `platformio.qualification.local.ini` mirrors the
tracked configuration with personal extra configs disabled. PlatformIO may replace
other build directories when switching configurations; retain simulator executable
copies outside `.pio/build` before running `pio check` or firmware builds.

## Resource and persistence review

The language policy is one byte of session state; lexical tables remain constexpr.
Classification and letter counting use bounded UTF-8 views and allocate no heap.
The selector keeps three fixed rows. Its activity allocation is checked with
`makeUniqueNoThrow`; cache-path strings and the Settings row are allocated only
when opening/rebuilding the existing settings UI. They preserve activity lifetime
and per-book context without retaining a global book reference. Preferences are
six bytes and are read at session entry/settings return; only changed choices
are written through temporary/backup promotion, never per display frame.
`book.bin` version 11 rebuilds old concatenated language metadata without deleting
reading progress, RSVP checkpoints or the new manual preference.

## Required device evaluation

On each physical X3/X4/X4 Pro, compare representative Russian and English passages
with grouping disabled/enabled at the same requested pace. Record subjective
fixation comfort, all four orientations, long active words and width/font fallback,
selector discoverability, no lost/repeated words, and reopen/sleep/restart behavior.
Confirm actual free heap remains above 50 KB without leaks. E-Ink BUSY/ghosting,
touch, SD/USB, battery and sleep remain unverified by these host/simulator results.

## Final firmware builds

All three release profiles passed without compiler warnings. Embedded model/version
strings and generated SHA-256 files were checked independently against each image.

| Model | Static RAM (bytes) | Application flash (bytes) |
| --- | ---: | ---: |
| X3 | 56744 | 5499127 |
| X4 | 56744 | 5499127 |
| X4PRO | 100384 | 5401722 |

Static RAM is not the free heap available during reading. All images fit the
6,553,600-byte application partition. Packages remain in ignored
`artifacts/crossrsvp-<model>-v0.7.0/firmware/`.

```text
1981c8b459c097107a56ec2224e0b57d5d13a7c339e36e5473df1775e74bc918  crossrsvp-x3-v0.7.0.bin
2ca747ac8f806155e83c5ff4c5079240d18628da63725b484523fa2685839e90  crossrsvp-x4-v0.7.0.bin
faa721abbfabea366029fdd2b7a6a39e4259b3e43dd3ce7cb51a69e1deb19aaa  crossrsvp-x4pro-v0.7.0.bin
```
