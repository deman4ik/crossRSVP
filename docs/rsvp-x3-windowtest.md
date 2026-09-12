# X3 RSVP window A/B diagnostic

This image is an experimental diagnostic for Xteink X3. It compares three
presentation variants: the current full B/W FAST update, a broad update
restricted to the RSVP line, and a tighter RSVP-line ROI update. It does not
change the normal firmware path or claim a new supported RSVP speed.
This qualification package is `0.10.0-windowtest.1`; the historical failure
below refers to the earlier `0.9.0-windowtest.1` diagnostic.

Before the sustained A/B run, the corrected diagnostic performs three short,
operator-confirmed probes. It shows a full-size PTL control and waits for a
visible Confirm, then performs a full resync, shows the reduced line-candidate
control, and waits for a second visible Confirm. After the existing full and
window warmup/measurement phases, it performs another full resync, shows the
tight ROI probe word `to`, and requires a fresh Confirm before its five-second
warmup and 60-second measurement. In the X3 driver trace, full-frame DTM2/DTM1
transfers stay outside partial mode and PTIN/PTL are used only around the FAST
refresh. This documents the protocol under test; it makes no claim of
SPI-frequency or SPI-transfer savings.

## Hardware results

### Current 0.10.0 physical result: all three branches completed

On 2026-09-12, a tester completed the `0.10.0-windowtest.1` diagnostic on one
X3 in portrait orientation. The CSV identifies `crossRSVP-v0.10.0-windowtest.1-x3`,
`UC8253`, and `assumed` controller confidence. All three branches completed;
the run recorded the three probe confirmations, no failures, and no fallbacks.
The hardware trace flag is `trace_valid=0`, because the physical run has no
simulator driver trace.

| Field | Full branch | Broad window branch | Tight window branch |
| --- | ---: | ---: | ---: |
| Measurement elapsed | 60,163 ms | 60,056 ms | 60,115 ms |
| Accepted measured frames | 114 | 130 | 130 |
| Whole-measurement rate | 113.691139/min | 129.878780/min | 129.751310/min |
| Reported interval-based frames/min | 115 | 132 | 132 |
| Refresh min / mean / max | 479 / 489 / 1,561 ms | 416 / 425 / 1,562 ms | 408 / 417 / 1,560 ms |
| Actual window / full / cleanup | 0 / 114 / 0 | 129 / 1 / 0 | 129 / 1 / 0 |
| ROI samples; width range; height range | 0; 0; 0 | 129; 120–120; 522–522 | 129; 96–96; 67–128 |

The whole-measurement rate uses accepted frames multiplied by 60,000 and
divided by the complete elapsed time. Compared with `full` in this same run,
`window` is +14.2383% and `tight_window` is +14.1261%; `tight_window` is
−0.0981% versus `window`. The tight branch therefore added no measurable
throughput over the broad branch in this run. The CSV's interval-based value
uses the 113 or 129 intervals between frames and is reported separately.

The raw user-provided CSV is preserved at
`artifacts/rsvp-x3-maxspeed-validation-v0.10.0/hardware-result-2026-09-12-user-01/rsvp-window-test.csv`.
Its SHA-256 is
`866f24d9b91ab72a4bc58af994e8942563545f89b93d366654b2c1fdf8063c86`.
The tester reported no traces of previous words; that is a visual observation,
not telemetry from the CSV. This is one physical unit and one orientation. It
does not qualify other X3 orientations, X4, X4 Pro, every controller revision,
or an absolute electrical panel limit.

### Current 0.9.1 hardware result: successful full/window run

On 2026-09-12, a tester completed the `0.9.1-windowtest.1` diagnostic on one
X3 in portrait orientation. The controller was `UC8253` with `assumed`
confidence. The full and broad-window rows completed without failures:

| Field | Full branch | Window branch |
| --- | --- | --- |
| Measurement elapsed | 60,163 ms | 60,108 ms |
| Accepted measured frames | 114 | 130 |
| Refresh min / mean / max | 479 / 489 / 1,561 ms | 416 / 425 / 1,561 ms |
| Reported interval-based frames/min | 115 | 132 |
| Actual window / full counts | 0 / 114 | 129 / 1 |
| Failures | 0 | 0 |

Using accepted frame count divided by the complete measurement duration, the
full branch delivered 113.68 frames/minute and the window branch delivered
129.77 frames/minute: a 14.1% higher wall-clock rate under this run's
conditions. The interval-based CSV rates use 113 and 129 intervals and are
reported separately above. This is one physical unit, one orientation, and
one controller-confidence result; it is not a general speed claim.

The raw user-provided CSV is preserved at
`artifacts/rsvp-x3-windowtest-validation-v0.9.1/hardware-result-2026-09-12-user-01/rsvp-window-test.csv`.
It contains no `tight_window` measurement; the later 0.10.0 three-branch result
above is the evidence for that variant.

### Historical 0.9.0 hardware result: failed visual qualification

On 2026-09-12, an earlier X3 run reported stripes appearing **only in the
window phase**, while the preceding full phase remained readable. The report
concerns the delivered `0.9.0-windowtest.1` diagnostic; the CSV has no
firmware-version field, so it cannot independently confirm the installed
version. This remains a historical failed visual result, not the current
hardware status and not evidence of a higher usable reading speed. Do not
extend or repeat runs that show stripes. Press Confirm to stop and export the
report, then use the previous readable X3 firmware for ordinary reading.

The historical physical failure has not been reproduced in the simulator. The
current host tests check emitted commands, payloads, and recovery state against
the expected implementation; they do not model the panel's interpretation of
partial writes. Their success does not establish that the tight variant works
on this X3.

Source comparison identifies an unverified difference: the candidate keeps PTIN
active across DTM2, refresh, and DTM1, whereas the pinned SDK's
`Uc8253X3Driver::writeGrayscalePlaneStrip` ends each partial RAM write with PTOUT.
That existing grayscale path does not itself prove the correct B/W sequence.
Window address/stride handling and the transition between RAM writes and refresh
remain hypotheses, not confirmed causes. The normal full phase working makes a
general installation or board-profile problem less likely.

The tester subsequently supplied `rsvp-window-test.csv` and clarified that the
word area contained sparse, faint black stripes. Paged reading was readable again
after leaving RSVP. The captured CSV reports:

| Field | Full branch | Window branch |
| --- | --- | --- |
| Controller / confidence / orientation | UC8253 / assumed / portrait | UC8253 / assumed / portrait |
| Requested pace | 300 words/min | 300 words/min |
| Measurement elapsed | 60,169 ms | 0 ms |
| Accepted measured frames | 114 | 0 |
| Refresh min / mean / max | 478 / 488 / 1,560 ms | no samples |
| Frame min / mean / max | 499 / 522 / 1,594 ms | no samples |
| Reported interval-based frames/min | 115 | no samples |
| Observed CPU | 160 MHz | no samples |
| Failures / last fallback | 0 / none | 1 / controller_error |

The full branch's whole-measurement rate is 113.68 frames/min (114 frames divided
by 60.169 seconds); the CSV's 115 uses the 113 intervals between accepted frames.
Neither value establishes a new production speed limit.

In this firmware, `windowFallback()` maps `BusyNotReady` to `controller_error`;
`BusyTimeout` and `InvalidRegion` have distinct report values. Thus the report
points to a readiness rejection before a display operation, not a recorded BUSY
timeout. It does not identify which preceding operation left the display busy.
Successful warmup frames are not counted, while warmup failures are, so zero
window samples do **not** prove that no window commands were sent. The delivered
report also omits the failure phase, requested update kind, and last successful
warmup operation. The corrected diagnostic adds `last_operation`,
`last_successful_operation`, `warmup_success_count`, and a `failure_*` snapshot
containing the phase, operation, requested and actual update kinds, error
name/code, duration, warmup successes, and the last successful operation before
failure. The snapshot remains in the CSV after a successful recovery redraw, so
recovery cannot hide the original `BusyNotReady`. The failed call in the old
report could therefore have been a full resync or a window call. Do not infer a
specific PTL defect from the old zero counters alone.

The historical raw CSV is preserved locally at
`artifacts/rsvp-x3-windowtest-validation/hardware-2026-09-12/rsvp-window-test.csv`.
The next diagnostic needs to distinguish those operations and preserve warmup
failure details without Serial. Do not remove the BUSY guard to make the test
continue. The reported return to readable paged mode confirms recovery of that
flow, not correctness of the partial update.

Before another long benchmark, retain this CSV. A photo showing the stripe pattern
and unaffected screen area remains useful if available. Whether stopping with
Confirm restores a readable results screen has not been reported. The next
hardware probe should separate a full-size PTL window from a reduced region,
with a full resync between probes, before testing sustained line updates. No
change to LUT, voltages, SPI frequency, or BUSY completion is justified by this
report.

Copy `rsvp-window-test.epub` to the SD card and open it in RSVP mode. The build
uses a deterministic built-in word sequence and never seeks or checkpoints the
EPUB, so the real reading position remains unchanged. Short-word grouping is
disabled for both branches. Font, orientation, guide style, FAST LUT, SPI, CPU,
power behavior, pacing, and cleanup cadence remain the same across the pair.
The diagnostic uses the maximum pacing sentinel (`target_wpm=0` in the CSV), so
the fixture does not impose a fixed words/minute cap. Actual throughput remains
bounded by the firmware main loop and scheduler; this measures delivered
firmware updates rather than a theoretical electrical panel maximum. The CSV
records the minimum and maximum CPU frequency observed for accepted measured frames. The simulator reports zero
samples because it has no CPU-frequency model. Power state is reported as
`not_sampled`; the diagnostic does not change the power policy.

The intro shows the selected controller and detection confidence. `assumed`
means the SDK selected UC8253 after an inconclusive probe; it is eligible only
for this diagnostic and is not hardware confirmation. UC8279, other controllers,
and `inconclusive` detection fall back to full updates and report that fact.

Hold Confirm for at least 0.7 seconds to start. The diagnostic then pauses after
each short probe for the required visual confirmation. Hold Confirm again on a
stopped result screen to repeat the pair. After the probes, the diagnostic runs,
in order:

1. full-size PTL probe and visual confirmation;
2. full resync, line-candidate probe, and visual confirmation;
3. full FAST warmup for 5 seconds;
4. full FAST measurement for at least 60 seconds;
5. window warmup for 5 seconds;
6. window measurement for at least 60 seconds;
7. full resync, tight ROI probe (`to`), and a fresh visual confirmation;
8. tight ROI warmup for 5 seconds;
9. tight ROI measurement for at least 60 seconds.

Each phase restarts the same fixed word sequence at its first word. The header is
static during a phase. Press Confirm during a run to abort. The display then shows
count, min/mean/max frame time, frames/minute, actual window/full counts, tight
ROI sample count and physical ROI min/max dimensions, and fallbacks. A complete
or aborted run writes
`/.crosspoint/rsvp-window-test.csv`; no SD write occurs while words are running.
An on-screen message reports a CSV write failure.
The CSV keeps the stable `full`, `window`, and `tight_window` rows and appends
`tight_probe_confirmed`, `roi_sample_count`, `roi_min_width`, `roi_max_width`,
`roi_min_height`, and `roi_max_height`. ROI dimensions are physical PTL extents;
the `full` row is zero, while `window` and `tight_window` record their own
successful actual-window samples. The tight row is zero when its branch has no
successful samples.

If a checked display operation times out, playback pauses without acknowledging
that Presentation Group or advancing its anchor. No further display command is
sent until the driver reports ready; the next presentation is a full resync.

## Physical qualification

Run the full three-branch test in each of the four orientations on the same charged device
under stable temperature and lighting. Record ghosting, contrast, line edges, and
readability after 5, 20, and 60 minutes. Those dwell times require repeated or
extended physical runs; the built-in roughly 220-second run alone does not cover them.
Keep the feature disabled by default unless repeated runs show a useful timing
gain with acceptable optical quality. The full branch uses the checked FAST path
with the same waveform and an observable completion/timeout result; this does not
claim every host wait instruction is bit-for-bit identical to an older build.
Software and simulator checks cover the expected command sequence and host state
transitions; they do not establish physical protocol compatibility, panel speed,
or image quality. The current 0.10.0 result qualifies its completed
full/window/tight portrait run on one X3. The historical 0.9.0 stripe report
remains relevant context, while other X3 orientations and X4/X4 Pro await their
own hardware checks.

The simulator harness captures the full-size, line-candidate, and tight probe
screens before their confirmations, checks that each probe is nonblank and that
the first two words transition, then runs all three measured branches for at
least 60 seconds in all four orientations. The tight probe displays the
expected `to` word and requires a fresh confirmation after the preceding full
resync; its pre-confirmation screenshot and paired measurement screenshots must
stay inside the existing RSVP line ROI. Because an uncapped deterministic run
can show the same word at two fixed offsets, the harness captures five unevenly
spaced tight measurement screenshots and validates the first pair that changes.
If every candidate pair is static, the run fails instead of treating a static
word as evidence of a working tight update.
For simulator runs, it also parses each successful `diagnostic_roi` rectangle,
checks the requested physical rectangle against the X3 bounds, matches the
window and tight CSV extrema to those trace entries, and requires the tight
rectangle to be smaller than the broad window rectangle. It verifies the exact
orientation stored in each CSV row, the
`tight_probe_confirmed` flag, and nonzero tight ROI samples with ordered
`roi_min_*`/`roi_max_*` dimensions. It rejects a blank display, a static word,
or any changed pixel outside the conservative central RSVP line band. The
paired screenshots and result screen remain evidence for manual review; the
harness does not perform optical panel qualification. Its abort scenario
injects a one-second simulator refresh and presses Confirm while rendering is
active, verifying that the input survives the render lock and that partial
results are exported.
The failure scenarios inject both a BUSY timeout and a single `BusyNotReady`
probe failure. They verify checked full recovery, the exact retained failure
phase/operation/error metadata, and that a short power-button attempt cannot
leave RSVP while the display recovery latch is still active. The deferred
scenario then requests the simulator's real `SLEEP` shortcut after recovery and
observes the intended Sleep activity. The simulator's `SLEEP` shortcut latches
the virtual power button; sending it while recovery is pending would prevent a
later short Back from being distinguished from a long press because the input
facade reports the longest held button. Pending-Sleep ordering therefore remains
a hardware/source-guard check rather than a simulator claim; the harness keeps
the deterministic Back-recovery boundary and post-recovery Sleep evidence.

Build commands:

```sh
pio run -e rsvp_x3_window_test
pio run -e simulator_x3_window_test -t run_simulator
python3 scripts/qualify_rsvp_windowtest.py \
  --program .pio/build/simulator_x3_window_test/program \
  --output artifacts/rsvp-x3-windowtest-validation
```

## Краткая инструкция

1. Сохраните копию SD-карты и файл предыдущей прошивки X3. Сверьте SHA-256
   диагностического `.bin` с `SHA256SUMS`.
2. Скопируйте `.bin` и `rsvp-window-test.epub` в корень SD-карты.
3. На X3 откройте **Настройки → Система → Обновление системы с SD-карты**,
   выберите диагностический файл, подтвердите обновление и дождитесь перезапуска.
4. Откройте `rsvp-window-test.epub` в режиме RSVP и удерживайте ОК не менее
   0,7 с. Дождитесь трёх 60-секундных замеров или нажмите ОК для прерывания.
5. Результат появится на экране и в `/.crosspoint/rsvp-window-test.csv`.
   Реальная позиция чтения не изменяется. Для отката выберите прежний `.bin`
   X3 через то же меню; не используйте прошивку X4 Pro.
