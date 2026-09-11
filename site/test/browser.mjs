import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { build } from '../build.mjs';

const dist = new URL('../dist/', import.meta.url);
const evidence = new URL('../../artifacts/site-copy-refresh/', import.meta.url);
await mkdir(evidence, { recursive: true });
await build([]);
const server = createServer(async (req, res) => {
  const path = new URL(req.url, 'http://localhost').pathname;
  const name = path === '/crossRSVP/' ? 'index.html' : path.replace('/crossRSVP/', '');
  if (!['index.html', 'styles.css', 'script.js'].includes(name)) { res.writeHead(404); res.end(); return; }
  try {
    const data = await readFile(new URL(name, dist));
    res.writeHead(200, { 'Content-Type': name.endsWith('.css') ? 'text/css' : name.endsWith('.js') ? 'text/javascript' : 'text/html' });
    res.end(data);
  } catch { res.writeHead(500); res.end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const base = `http://127.0.0.1:${server.address().port}/crossRSVP/`;
const browser = await chromium.launch({ headless: true, ...(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {}) });
const errors = [];
async function open(options = {}, init) {
  const context = await browser.newContext({ locale: 'en-US', viewport: { width: 1440, height: 1000 }, ...options });
  await context.route('**/*', route => route.request().url().startsWith(base) ? route.continue() : route.abort());
  if (init) await context.addInitScript(init);
  const page = await context.newPage(); page.setDefaultTimeout(10000);
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(base, { waitUntil: 'domcontentloaded' });
  if (options.javaScriptEnabled !== false) await page.waitForFunction(() => document.documentElement.classList.contains('js'));
  return { context, page };
}
const text = async (page, selector) => (await page.locator(selector).textContent()).trim();
const paused = async page => assert.match(await text(page, '#demo-state'), /^(Paused|Пауза)$/);
try {
  console.log('browser: language detection, both directions, persisted choice');
  let { context, page } = await open({ locale: 'ru-RU' });
  assert.equal(await page.locator('html').getAttribute('lang'), 'ru');
  assert.match(await text(page, 'h1'), /Читайте/);
  await page.locator('#language').click();
  assert.match(await text(page, 'h1'), /Read/);
  assert.match(await text(page, '#downloads'), /No firmware file for this model yet/);
  await page.reload({ waitUntil: 'domcontentloaded' });
  assert.equal(await page.locator('html').getAttribute('lang'), 'en');
  await page.locator('#language').click();
  assert.match(await text(page, '#downloads'), /Для этой модели пока нет файла прошивки/);
  assert.doesNotMatch(await text(page, 'body'), /undefined|No firmware file|Download firmware/);
  await context.close();
  for (const init of [() => Object.defineProperty(window, 'localStorage', { get() { throw new Error('blocked'); } }),
    () => localStorage.setItem('crossrsvp-language', 'invalid')]) {
    ({ context, page } = await open({ locale: 'ru-RU' }, init));
    assert.equal(await page.locator('html').getAttribute('lang'), 'ru');
    await page.locator('#language').click(); assert.match(await text(page, 'h1'), /Read/);
    await context.close();
  }

  console.log('browser: words, fixed ORP, pause/continue, pace, finish/restart');
  ({ context, page } = await open());
  await page.clock.install();
  await paused(page); const initial = await text(page, '#demo-word');
  await page.clock.runFor(1800); assert.equal(await text(page, '#demo-word'), initial);
  const center = async () => { const box = await page.locator('.word-pivot').boundingBox(); return box.x + box.width / 2; };
  const anchor = await center();
  await page.locator('#demo-toggle').click();
  await page.clock.runFor(650); assert.equal(await text(page, '#demo-word'), 'can');
  assert.ok(Math.abs(await center() - anchor) < 1);
  await page.locator('#demo-toggle').click(); const current = await text(page, '#demo-word');
  await page.clock.runFor(1800); await paused(page); assert.equal(await text(page, '#demo-word'), current);
  await page.locator('#demo-toggle').click(); assert.equal(await text(page, '#demo-word'), current);
  await page.clock.runFor(650); assert.equal(await text(page, '#demo-word'), 'read');
  assert.ok(Math.abs(await center() - anchor) < 1);
  await page.locator('#speed').focus(); await page.keyboard.press('End');
  assert.equal(await page.locator('#speed').inputValue(), '180'); assert.equal(await text(page, '#speed-value'), '180');
  await page.clock.runFor(6000); assert.equal(await text(page, '#demo-state'), 'Finished');
  assert.equal(await text(page, '#demo-word'), 'need.');
  await page.clock.runFor(1200); assert.equal(await text(page, '#demo-word'), 'need.');
  await page.locator('#demo-reset').click(); await paused(page); assert.equal(await text(page, '#demo-word'), 'You');
  await page.locator('#demo-toggle').click(); await page.locator('#language').click(); await paused(page);
  assert.equal(await text(page, '#demo-word'), 'Вы');
  await page.clock.runFor(1000); await paused(page);
  await page.locator('#demo-toggle').click();
  await page.evaluate(() => { Object.defineProperty(document, 'hidden', { configurable: true, value: true }); document.dispatchEvent(new Event('visibilitychange')); });
  await paused(page);
  await page.evaluate(() => { Object.defineProperty(document, 'hidden', { configurable: true, value: false }); document.dispatchEvent(new Event('visibilitychange')); });
  await page.clock.runFor(1000); await paused(page);
  assert.equal(await page.locator('#demo-word').getAttribute('aria-live'), 'off');
  await context.close();

  console.log('browser: keyboard focus, mobile overflow, reduced motion, screenshots');
  await build([{ tag_name: 'v0.6.0', draft: false, prerelease: true, published_at: '2026-09-01T00:00:00Z',
    html_url: 'https://github.com/deman4ik/crossRSVP/releases/tag/v0.6.0',
    assets: ['x3', 'x4', 'x4pro'].map(model => ({ name: `crossrsvp-${model}-v0.6.0.bin`, browser_download_url: `https://example.test/crossrsvp-${model}-v0.6.0.bin` })) }]);
  for (const width of [320, 390, 768, 1024, 1440]) {
    ({ context, page } = await open({ viewport: { width, height: 900 }, locale: 'ru-RU', reducedMotion: 'reduce' }));
    await page.keyboard.press('Tab'); assert.equal(await page.locator(':focus').getAttribute('href'), '#main');
    const focus = await page.locator(':focus').evaluate(node => ({ outline: getComputedStyle(node).outlineStyle, width: getComputedStyle(node).outlineWidth }));
    assert.notEqual(focus.outline, 'none'); assert.notEqual(focus.width, '0px');
    assert.equal(await page.evaluate(() => getComputedStyle(document.documentElement).scrollBehavior), 'auto');
    for (const language of ['ru', 'en']) {
      if (language === 'en') await page.locator('#language').click();
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true, `overflow at ${width} in ${language}`);
      for (const selector of ['.hero-copy', '.feature-grid', '.demo-intro', '.demo-card', '.device-grid', '.install-steps']) {
        const box = await page.locator(selector).boundingBox();
        assert.ok(box.x >= 23 && box.x + box.width <= width - 23, `missing side gutters at ${width}: ${selector}`);
      }
      assert.equal(await page.locator('.beta-badge').count(), 3);
      assert.equal(await page.locator('.device-card a[href$=".bin"]').count(), 3);
      assert.equal(await page.locator('#install li').count(), 4);
      assert.doesNotMatch(await text(page, 'body'), /undefined|Спокойная страница|Одна опора|Стабильные сборки/);
    }
    await page.locator('#language').click();
    await page.keyboard.press('Escape'); await page.locator('h1').click();
    await page.screenshot({ path: fileURLToPath(new URL(`site-${width}.png`, evidence)), fullPage: true });
    await context.close();
  }

  console.log('browser: static downloads and available cards in both languages');
  await build([{ tag_name: 'v9.0.0', draft: false, prerelease: true, published_at: '2026-09-01T00:00:00Z',
    html_url: 'https://github.com/deman4ik/crossRSVP/releases/tag/v9.0.0',
    assets: [{ name: 'crossrsvp-x3-v2.3.4.bin', browser_download_url: 'https://example.test/crossrsvp-x3-v2.3.4.bin' }] }]);
  ({ context, page } = await open({ javaScriptEnabled: false }));
  assert.match(await text(page, 'body'), /Enable JavaScript/);
  assert.equal(await page.locator('a[href="https://example.test/crossrsvp-x3-v2.3.4.bin"]').count(), 1);
  assert.equal(await page.locator('#demo-toggle').isVisible(), false);
  assert.equal(await page.locator('.beta-badge').count(), 1);
  assert.match(await text(page, '#install'), /SD Card Firmware Update/);
  await context.close();
  ({ context, page } = await open({ locale: 'ru-RU' }));
  assert.equal(await page.getByRole('link', { name: 'Скачать .bin ↓' }).count(), 1);
  await page.locator('#language').click(); assert.equal(await page.getByRole('link', { name: 'Download .bin', exact: true }).count(), 1);
  assert.match(await text(page, '#install'), /SD Card Firmware Update/);
  await page.getByRole('link', { name: 'Install', exact: true }).click();
  assert.equal(new URL(page.url()).hash, '#install');
  assert.deepEqual(errors, []);
  await context.close();
  console.log('Browser checks PASS');
} finally {
  await browser.close(); await new Promise(resolve => server.close(resolve));
  await build([]);
}
