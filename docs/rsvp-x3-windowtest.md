# X3 RSVP window A/B diagnostic

This image is an experimental diagnostic for Xteink X3. It compares the current
full B/W FAST presentation with a candidate update restricted to the RSVP line.
It does not change the normal firmware path or claim a new supported RSVP speed.

Copy `rsvp-window-test.epub` to the SD card and open it in RSVP mode. The build
uses a deterministic built-in word sequence and never seeks or checkpoints the
EPUB, so the real reading position remains unchanged. Short-word grouping is
disabled for both branches. Font, orientation, guide style, FAST LUT, SPI, CPU,
power behavior, pacing, and cleanup cadence remain the same across the pair.
The fixed target is 300 words/min. The CSV records the minimum and maximum CPU
frequency observed for accepted measured frames. The simulator reports zero
samples because it has no CPU-frequency model. Power state is reported as
`not_sampled`; the diagnostic does not change the power policy.

The intro shows the selected controller and detection confidence. `assumed`
means the SDK selected UC8253 after an inconclusive probe; it is eligible only
for this diagnostic and is not hardware confirmation. UC8279, other controllers,
and `inconclusive` detection fall back to full updates and report that fact.

Hold Confirm for at least 0.7 seconds to start. Hold it again on a stopped result
screen to repeat the pair. The diagnostic runs, in order:

1. full FAST warmup for 5 seconds;
2. full FAST measurement for at least 60 seconds;
3. window warmup for 5 seconds;
4. window measurement for at least 60 seconds.

Each phase restarts the same fixed word sequence at its first word. The header is
static during a phase. Press Confirm during a run to abort. The display then shows
count, min/mean/max frame time, frames/minute, actual window/full counts, and
fallbacks. A complete or aborted run writes
`/.crosspoint/rsvp-window-test.csv`; no SD write occurs while words are running.
An on-screen message reports a CSV write failure.

If a checked display operation times out, playback pauses without acknowledging
that Presentation Group or advancing its anchor. No further display command is
sent until the driver reports ready; the next presentation is a full resync.

## Physical qualification

Run the full A/B test in each of the four orientations on the same charged device
under stable temperature and lighting. Record ghosting, contrast, line edges, and
readability after 5, 20, and 60 minutes. Those dwell times require repeated or
extended physical runs; the built-in 130-second pair alone does not cover them.
Keep the feature disabled by default unless repeated runs show a useful timing
gain with acceptable optical quality. The full branch uses the checked FAST path
with the same waveform and an observable completion/timeout result; this does not
claim every host wait instruction is bit-for-bit identical to an older build.
Software and simulator checks establish protocol/state correctness; they do not
establish the panel's physical speed or image quality.

The simulator harness runs both measured branches for at least 60 seconds in
all four orientations, verifies the exact orientation stored in each CSV row,
and compares two changing-word screenshots from each window phase. It rejects
a blank display, a static word, or any changed pixel outside the conservative
central RSVP line band. The paired full/window screenshots and result screen
remain evidence for manual review; the harness does not perform optical panel
qualification. Its abort scenario injects a one-second simulator refresh and
presses Confirm while rendering is active, verifying that the input survives
the render lock and that partial results are exported.
The failure scenarios also inject a BUSY timeout, verify checked full recovery,
and verify that queued Back, Home, and Sleep actions cannot leave RSVP while the
display recovery latch is still active.

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
   0,7 с. Дождитесь двух 60-секундных замеров или нажмите ОК для прерывания.
5. Результат появится на экране и в `/.crosspoint/rsvp-window-test.csv`.
   Реальная позиция чтения не изменяется. Для отката выберите прежний `.bin`
   X3 через то же меню; не используйте прошивку X4 Pro.
