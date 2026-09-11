import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { build, fetchReleases } from '../build.mjs';

test('empty or prerelease-only input keeps every model clearly unavailable', async () => {
  await build([{ tag_name:'v0.6.0', prerelease:true, draft:false, html_url:'https://notes', assets:[{name:'crossrsvp-x3-v0.6.0.bin',browser_download_url:'https://download'}] }]);
  const html = await readFile(new URL('../dist/index.html', import.meta.url), 'utf8');
  assert.equal((html.match(/Stable release not published yet/g) || []).length, 3);
  assert.doesNotMatch(html, /crossrsvp-x3-v0\.6\.0\.bin/);
});
test('selects exact model assets and preserves independent versions', async () => {
  const release = (tag, date, assets) => ({tag_name:tag, prerelease:false, draft:false, published_at:date, html_url:`https://github.com/deman4ik/crossRSVP/releases/tag/${tag}`, assets:assets.map(name=>({name,browser_download_url:`https://example.test/${name}`}))});
  await build([release('v1.0.0','2025-01-01',['crossrsvp-x4-v1.0.0.bin']), release('v2.0.0','2026-01-01',['crossrsvp-x3-v2.0.0.bin','crossrsvp-x4pro-v2.0.0.bin'])]);
  const html = await readFile(new URL('../dist/index.html', import.meta.url), 'utf8');
  assert.match(html, /example\.test\/crossrsvp-x3-v2\.0\.0\.bin/); assert.match(html, /example\.test\/crossrsvp-x4-v1\.0\.0\.bin/); assert.match(html, /example\.test\/crossrsvp-x4pro-v2\.0\.0\.bin/); assert.doesNotMatch(html, /firmware\.bin/);
});
test('rejects ambiguous matching assets', async () => { await assert.rejects(() => build([{tag_name:'v1.0.0',draft:false,prerelease:false,published_at:'2026-01-01',html_url:'https://notes',assets:[{name:'crossrsvp-x3-v1.0.0.bin',browser_download_url:'https://a'},{name:'crossrsvp-x3-v1.0.0.bin',browser_download_url:'https://b'}]}]), /Multiple matching/); });
test('supports paginated API responses and chooses newest release by date', async () => {
  const original = globalThis.fetch; let calls = 0;
  globalThis.fetch = async url => { calls++; return { ok:true, json:async()=>calls===1 ? Array.from({length:100},(_,i)=>({tag_name:`v${i}.0.0`,draft:false,prerelease:false,published_at:'2026-01-01',html_url:'https://notes',assets:[]})) : [{tag_name:'v2.0.0',draft:false,prerelease:false,published_at:'2025-01-01',html_url:'https://notes',assets:[{name:'crossrsvp-x3-v2.0.0.bin',browser_download_url:'https://download'}]}] }; };
  try { const releases=await fetchReleases(); assert.equal(calls,2); await build(releases); const html=await readFile(new URL('../dist/index.html', import.meta.url),'utf8'); assert.match(html,/https:\/\/download/); } finally { globalThis.fetch=original; }
});
test('fails on network, rate limit, and malformed API responses', async () => {
  const original=globalThis.fetch;
  for (const response of [Promise.reject(new Error('network')), {ok:false,status:429}, {ok:true,json:async()=>({error:'bad'})}]) { globalThis.fetch=()=>response; await assert.rejects(()=>fetchReleases()); }
  globalThis.fetch=original;
});
test('escapes hostile release metadata before embedding HTML', async () => {
  await build([{tag_name:'v1.0.0',draft:false,prerelease:false,published_at:'2026-01-01',html_url:'https://example.test/"onmouseover="x',assets:[{name:'crossrsvp-x3-v1.0.0.bin',browser_download_url:'https://example.test/a?x="&y=<'}]}]);
  const html=await readFile(new URL('../dist/index.html', import.meta.url),'utf8'); assert.doesNotMatch(html,/onmouseover="x/); assert.match(html,/&quot;/);
});

const sample = (tag, date, names, extra = {}) => ({
  tag_name: tag, draft: false, prerelease: false, published_at: date,
  html_url: `https://github.com/deman4ik/crossRSVP/releases/tag/${tag}`,
  assets: names.map(name => ({ name, browser_download_url: `https://example.test/${name}` })), ...extra
});
const htmlOutput = () => readFile(new URL('../dist/index.html', import.meta.url), 'utf8');

test('empty and draft-only catalogs do not manufacture downloads', async () => {
  for (const releases of [[], [sample('v9.0.0', null, ['crossrsvp-x3-v9.0.0.bin'], { draft: true })]]) {
    await build(releases); const html = await htmlOutput();
    assert.equal((html.match(/Stable release not published yet/g) || []).length, 3);
    assert.doesNotMatch(html, /href="[^"\n]*\.bin"/);
  }
});
test('publication order, image version and unrelated filenames are handled independently', async () => {
  await build([
    sample('v9.0.0', '2026-02-01T00:00:00Z', ['crossrsvp-x3-v2.3.4.bin']),
    sample('v1.0.0', '2026-01-01T00:00:00Z', ['crossrsvp-x3-v1.0.0.bin', 'crossrsvp-x4-v1.0.0.bin']),
    sample('v10.0.0', '2026-03-01T00:00:00Z', ['crossrsvp-x4pro-v10.0.0.bin', 'firmware.bin', 'crossrsvp-x4c-v10.0.0.bin', 'crossrsvp-x4-v10.0.0.bin.zip'])
  ]);
  const html = await htmlOutput();
  assert.match(html, /<strong>v2\.3\.4<\/strong> · 2026-02-01/);
  assert.match(html, /example\.test\/crossrsvp-x4-v1\.0\.0\.bin/);
  assert.match(html, /example\.test\/crossrsvp-x4pro-v10\.0\.0\.bin/);
  assert.doesNotMatch(html, /example\.test\/(?:firmware|crossrsvp-x4c|crossrsvp-x4-v10)/);
});
test('malformed metadata and unsafe URLs fail before replacing the previous HTML', async () => {
  await build([]); const previous = await htmlOutput();
  const good = sample('v1.0.0', '2026-01-01', ['crossrsvp-x3-v1.0.0.bin']);
  for (const value of [null, {}, { ...good, assets: null }, { ...good, published_at: 'yesterday' },
    { ...good, html_url: 'javascript:alert(1)' },
    { ...good, assets: [{ name: 'crossrsvp-x3-v1.0.0.bin', browser_download_url: 'data:text/html,hello' }] }]) {
    await assert.rejects(() => build([value]));
    assert.equal(await htmlOutput(), previous);
  }
});
test('two versions of the same model in one release are ambiguous', async () => {
  await assert.rejects(() => build([sample('v2.0.0', '2026-01-01',
    ['crossrsvp-x3-v1.0.0.bin', 'crossrsvp-x3-v2.0.0.bin'])]), /Multiple matching/);
});
test('authenticated API access never embeds credentials in generated content', async () => {
  const original = globalThis.fetch, token = process.env.GITHUB_TOKEN;
  process.env.GITHUB_TOKEN = 'fixture-secret-not-for-html';
  globalThis.fetch = async (url, options) => {
    assert.equal(options.headers.Authorization, 'Bearer fixture-secret-not-for-html');
    assert.match(url, /per_page=100&page=1/);
    return { ok: true, json: async () => [] };
  };
  try { await build(await fetchReleases()); assert.doesNotMatch(await htmlOutput(), /fixture-secret-not-for-html/); }
  finally { globalThis.fetch = original; if (token === undefined) delete process.env.GITHUB_TOKEN; else process.env.GITHUB_TOKEN = token; }
});
