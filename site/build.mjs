import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('.', import.meta.url));
const out = join(root, 'dist');
const repo = 'deman4ik/crossRSVP';
const apiBase = process.env.RELEASES_API || `https://api.github.com/repos/${repo}/releases`;
const models = [
  ['x3', 'Xteink X3', 'crossrsvp-x3-v'],
  ['x4', 'Xteink X4', 'crossrsvp-x4-v'],
  ['x4pro', 'Xteink X4 Pro', 'crossrsvp-x4pro-v']
];

const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export async function fetchReleases() {
  if (process.env.RELEASES_FILE) return JSON.parse(await readFile(process.env.RELEASES_FILE, 'utf8'));
  const all = [];
  for (let page = 1; page < 100; page++) {
    const response = await fetch(`${apiBase}?per_page=100&page=${page}`, { headers: { Accept: 'application/vnd.github+json', 'User-Agent': 'crossrsvp-pages', ...(process.env.GITHUB_TOKEN ? { Authorization: `Bearer ${process.env.GITHUB_TOKEN}` } : {}) } });
    if (!response.ok) throw new Error(`GitHub releases API returned ${response.status}`);
    const data = await response.json();
    if (!Array.isArray(data)) throw new Error('GitHub releases API returned invalid JSON');
    data.forEach(validateRelease);
    all.push(...data);
    if (data.length < 100) return all;
  }
  throw new Error('Release pagination exceeded the supported limit');
}
function validUrl(value) {
  if (typeof value !== 'string') return false;
  try { const url = new URL(value); return url.protocol === 'https:' && !url.username && !url.password; }
  catch { return false; }
}
function validateRelease(release) {
  if (!release || typeof release !== 'object' || typeof release.tag_name !== 'string' ||
      typeof release.draft !== 'boolean' || typeof release.prerelease !== 'boolean' ||
      !Array.isArray(release.assets) || !validUrl(release.html_url)) {
    throw new Error('GitHub releases API returned malformed release data');
  }
  if (!release.draft &&
      (typeof release.published_at !== 'string' || !Number.isFinite(Date.parse(release.published_at)))) {
    throw new Error('Release has an invalid publication date');
  }
  for (const asset of release.assets) {
    if (!asset || typeof asset.name !== 'string' || !validUrl(asset.browser_download_url)) {
      throw new Error('Release has malformed asset data');
    }
  }
}
export function selectRelease(releases, prefix) {
  const pattern = new RegExp(`^${prefix}(\\d+\\.\\d+\\.\\d+(?:[-+][0-9A-Za-z.-]+)?)\\.bin$`);
  const matches = [];
  for (const release of releases) {
    if (release.draft) continue;
    const assets = release.assets.filter(asset => pattern.test(asset.name));
    if (assets.length > 1) throw new Error(`Multiple matching assets found for ${prefix}`);
    if (assets.length) matches.push({ release, asset: assets[0] });
  }
  matches.sort((a, b) => Date.parse(b.release.published_at) - Date.parse(a.release.published_at));
  if (!matches.length) return null;
  if (matches.length > 1 && Date.parse(matches[0].release.published_at) === Date.parse(matches[1].release.published_at)) {
    throw new Error(`Ambiguous publication date for ${prefix}`);
  }
  const { release, asset } = matches[0];
  return { version: `v${asset.name.match(pattern)[1]}`, date: release.published_at,
           notes: release.html_url, url: asset.browser_download_url, beta: release.prerelease };
}
function card(model, release) {
  if (!release) return `<article class="device-card unavailable"><div class="device-icon" aria-hidden="true">${model[0].toUpperCase()}</div><h3>${esc(model[1])}</h3><p class="status" data-i18n="unavailable">No firmware file for this model yet.</p><a data-i18n="view" href="https://github.com/${repo}/releases">View releases <span aria-hidden="true">↗</span></a></article>`;
  return `<article class="device-card"><div class="device-icon" aria-hidden="true">${model[0].toUpperCase()}</div><h3>${esc(model[1])}</h3><p class="release-meta"><strong>${esc(release.version)}</strong> · ${esc(release.date.slice(0, 10))}${release.beta ? ' <span class="beta-badge">beta</span>' : ''}</p><div class="card-actions"><a data-i18n="download" class="button small" href="${esc(release.url)}">Download .bin <span aria-hidden="true">↓</span></a><a data-i18n="notes" class="text-link" href="${esc(release.notes)}">What’s new ↗</a></div></article>`;
}
export async function build(releases) {
  if (!Array.isArray(releases)) throw new Error('Release input must be an array');
  releases.forEach(validateRelease);
  const template = await readFile(join(root, 'src/index.html'), 'utf8');
  const cards = models.map(m => card(m, selectRelease(releases, m[2]))).join('\n');
  await mkdir(out, { recursive: true });
  await writeFile(join(out, 'index.html'), template.replace('<!-- RELEASE_CARDS -->', cards));
  await writeFile(join(out, 'styles.css'), await readFile(join(root, 'src/styles.css')));
  await writeFile(join(out, 'script.js'), await readFile(join(root, 'src/script.js')));
}
if (import.meta.url === `file://${process.argv[1]}`) build(await fetchReleases()).catch(error => { console.error(error.message); process.exitCode = 1; });
