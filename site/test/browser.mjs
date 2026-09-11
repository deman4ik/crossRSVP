import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { build } from '../build.mjs';

const dist = new URL('../dist/', import.meta.url);
const evidence = new URL('../../artifacts/issues-13-14/', import.meta.url);
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
  assert.match(await text(page, '#downloads'), /Stable release not published yet/);
  await page.reload({ waitUntil: 'domcontentloaded' });
  assert.equal(await page.locator('html').getAttribute('lang'), 'en');
  await page.locator('#language').click();
  assert.match(await text(page, '#downloads'), /Стабильный релиз пока не опубликован/);
  assert.doesNotMatch(await text(page, 'body'), /undefined|Stable release not published|Choose your device/);
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
  await page.clock.runFor(650); assert.equal(await text(page, '#demo-word'), 'calm');
  assert.ok(Math.abs(await center() - anchor) < 1);
  await page.locator('#demo-toggle').click(); const current = await text(page, '#demo-word');
  await page.clock.runFor(1800); await paused(page); assert.equal(await text(page, '#demo-word'), current);
  await page.locator('#demo-toggle').click(); assert.equal(await text(page, '#demo-word'), current);
  await page.clock.runFor(650); assert.equal(await text(page, '#demo-word'), 'page');
  assert.ok(Math.abs(await center() - anchor) < 1);
  await page.locator('#speed').focus(); await page.keyboard.press('End');
  assert.equal(await page.locator('#speed').inputValue(), '180'); assert.equal(await text(page, '#speed-value'), '180');
  await page.clock.runFor(6000); assert.equal(await text(page, '#demo-state'), 'Finished');
  assert.equal(await text(page, '#demo-word'), 'time.');
  await page.clock.runFor(1200); assert.equal(await text(page, '#demo-word'), 'time.');
  await page.locator('#demo-reset').click(); await paused(page); assert.equal(await text(page, '#demo-word'), 'A');
  await page.locator('#demo-toggle').click(); await page.locator('#language').click(); await paused(page);
  assert.equal(await text(page, '#demo-word'), 'Спокойная');
  await page.clock.runFor(1000); await paused(page);
  await page.locator('#demo-toggle').click();
  await page.evaluate(() => { Object.defineProperty(document, 'hidden', { configurable: true, value: true }); document.dispatchEvent(new Event('visibilitychange')); });
  await paused(page);
  await page.evaluate(() => { Object.defineProperty(document, 'hidden', { configurable: true, value: false }); document.dispatchEvent(new Event('visibilitychange')); });
  await page.clock.runFor(1000); await paused(page);
  assert.equal(await page.locator('#demo-word').getAttribute('aria-live'), 'off');
  await context.close();

  console.log('browser: keyboard focus, mobile overflow, reduced motion, screenshots');
  for (const width of [320, 390, 1440]) {
    ({ context, page } = await open({ viewport: { width, height: 900 }, locale: width === 1440 ? 'en-US' : 'ru-RU', reducedMotion: 'reduce' }));
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true, `overflow at ${width}`);
    await page.keyboard.press('Tab'); assert.equal(await page.locator(':focus').getAttribute('href'), '#main');
    const focus = await page.locator(':focus').evaluate(node => ({ outline: getComputedStyle(node).outlineStyle, width: getComputedStyle(node).outlineWidth }));
    assert.notEqual(focus.outline, 'none'); assert.notEqual(focus.width, '0px');
    assert.equal(await page.evaluate(() => getComputedStyle(document.documentElement).scrollBehavior), 'auto');
    await page.keyboard.press('Escape'); await page.locator('h1').click();
    await page.screenshot({ path: fileURLToPath(new URL(`site-${width}.png`, evidence)), fullPage: true });
    await context.close();
  }

  console.log('browser: static downloads and available cards in both languages');
  await build([{ tag_name: 'v9.0.0', draft: false, prerelease: false, published_at: '2026-09-01T00:00:00Z',
    html_url: 'https://github.com/deman4ik/crossRSVP/releases/tag/v9.0.0',
    assets: [{ name: 'crossrsvp-x3-v2.3.4.bin', browser_download_url: 'https://example.test/crossrsvp-x3-v2.3.4.bin' }] }]);
  ({ context, page } = await open({ javaScriptEnabled: false }));
  assert.match(await text(page, 'body'), /Enable JavaScript/);
  assert.equal(await page.locator('a[href="https://example.test/crossrsvp-x3-v2.3.4.bin"]').count(), 1);
  assert.equal(await page.locator('#demo-toggle').isVisible(), false);
  await context.close();
  ({ context, page } = await open({ locale: 'ru-RU' }));
  assert.equal(await page.getByRole('link', { name: 'Скачать .bin ↓' }).count(), 1);
  await page.locator('#language').click(); assert.equal(await page.getByRole('link', { name: 'Download .bin', exact: true }).count(), 1);
  assert.deepEqual(errors, []);
  await context.close();
  console.log('Browser checks PASS');
} finally {
  await browser.close(); await new Promise(resolve => server.close(resolve));
  await build([]);
}
