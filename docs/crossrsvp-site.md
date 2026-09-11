# CrossRSVP Pages site

The public site lives in `site/` and produces static HTML, CSS, and JavaScript. It is independent of the firmware build. GitHub Pages is configured to deploy the `site/dist` artifact through `.github/workflows/site.yml`.

## Local build

From the repository root:

```sh
cd site
npm ci
npm test
npx playwright install chromium
npm run test:browser
RELEASES_FILE=test/fixtures/releases.json npm run build
python3 -m http.server 4173 --directory dist
```

The browser checks serve the real build under `/crossRSVP/`, block external font requests for repeatability, and save desktop/mobile screenshots to ignored `artifacts/site-copy-refresh/`. To use an existing local browser, set `CHROMIUM_PATH` to its executable. The checks cover both translation directions, stored/blocked preferences, word playback and a fixed ORP, pause/continue, pace, completion/restart, tab visibility, keyboard focus, reduced motion, mobile overflow, and downloads without JavaScript.

The fixture is intentionally an empty release list, so local output shows an honest unavailable state. A live build reads all pages from the public GitHub Releases API. Drafts and assets with a non-exact model filename are excluded. Published prereleases are included and carry a visible `beta` label. Each model links directly to its newest available image by publication date, whether stable or beta. If the API fails or returns malformed JSON, the build exits before the Pages artifact is uploaded; the previous deployment remains intact.

The generated links work from a repository Pages sub-path because the site uses document-relative assets. Release metadata is embedded at build time, so visitors do not need JavaScript or a GitHub API request to see downloads.

## Maintainer checklist

Enable **Settings → Pages → Source: GitHub Actions** once. The workflow runs on changes to `site/**` in `master`, published releases, and manual dispatch. All build and browser checks must pass before deployment. Use the workflow dispatch button after correcting release attachments or deleting a release. Download cards only appear for exact names `crossrsvp-x3-v<version>.bin`, `crossrsvp-x4-v<version>.bin`, and `crossrsvp-x4pro-v<version>.bin`; prereleases appear immediately with a `beta` label.

The visual language takes broad inspiration from the CrossPoint Tools site. CrossRSVP is an independent fork; links and attribution are included in the footer.

## Release catalog checks

The 12 generator scenarios cover empty/draft/prerelease catalogs, exact independent
model assets and image versions, date ordering, unrelated filenames, ambiguous
assets, pagination, malformed dates/objects/URLs, API/network/rate-limit failures,
HTML escaping, and keeping the API token out of output. Failure occurs before
artifact publication. Availability is determined from public GitHub
Releases, not local firmware packages; v0.6.0 is available with a beta label.

The HTML/CSS illustration and copy are original to this site. Broad visual
inspiration (light surface, green accents, serif headings) and the upstream
project are credited in the footer; no upstream testimonials, statistics,
third-party endorsement logos, or analytics were copied.

## Copy and layout

The RU/EN page introduces the firmware before explaining five reading features.
The demo has its own short instructions; optional word grouping is explained in a
feature card. Installation has four SD-card update
steps, a separate route for readers still on factory firmware, and the full guide.
The shared content width and responsive side gutters apply to every section.
Browser checks verify both languages at 320, 390, 768, 1024 and 1440 px, including
actual left/right clearances, beta downloads, installation links and static content.
