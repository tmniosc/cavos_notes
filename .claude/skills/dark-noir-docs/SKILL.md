---
name: dark-noir-docs
description: >-
  Write documentation as HTML in the house "dark noir" theme: black, white and
  greys (one red for danger), square corners everywhere, condensed uppercase
  title-card headings, a sticky sidebar (site map + contents of the page with
  scroll-spy), colour-highlighted copyable code blocks, tables, callouts and
  embedded SVG diagrams. Works for one self-contained page or for a whole
  multi-page site that opens offline from file://. Use it whenever the user
  wants docs, notes, a README, a guide, a manual, a cheat sheet or a study
  vault written or rewritten as HTML; when they say "viết bằng html", "soạn
  lại doc bằng html", "chuyển note sang html", "README html", "dark theme không
  bo góc", "kiểu noir", "theme docs dark noir"; when a Markdown/Obsidian vault
  should become HTML pages; or when adding a new page to a site already built
  with this theme (it has assets/noir.css + assets/site.js). Prefer it over
  plain Markdown for docs meant to be read. Do NOT use it for the literary
  "noir-page" look (Playfair/Lora, drifting dust) — that is a different skill.
---

# Dark noir docs

Docs as HTML, in one look. The theme came from real asks: Markdown READMEs
were "hard to read", so the user wanted HTML, dark, no rounded corners, then
"kiểu noir" (black/white/grey, colour only in code). Later a whole Obsidian
study vault was moved onto it, so the theme now covers multi-page sites too.

The theme is a **look**, not a content template: structure each page around
what it explains. Files in `assets/`:

| File | What it is |
| --- | --- |
| `noir.css` | the whole theme (tokens, layout, components) |
| `noir.js` | syntax colours, Copy buttons, sidebar (site map + page contents), scroll-spy, prev/next |
| `page.html` | skeleton of one page with an example of every component |
| `fonts/` | IBM Plex Mono woff2 (12 files, ~140 KB) + `LICENSE` (OFL) |

`scripts/obsidian_to_noir.py` converts an Obsidian vault (wikilinks, embeds,
callouts, front matter) into a site in this theme.

## Two ways to ship

**Single page** (a README, one guide). Copy `page.html`, paste the contents of
`noir.css` into a `<style>` and of `noir.js` into a `<script>` at the end of
`<body>`, drop the `site.js` script tag, and put the `fonts/` folder next to
the page (the `@font-face` URLs are `fonts/…`). Opens with a double click.

**Site** (a vault, a set of pages). Keep the files shared:

```
<project>/docs/
├── index.html            home page
├── assets/noir.css       copied from the skill
├── assets/noir.js        copied from the skill
├── assets/fonts/         copied from the skill
├── assets/site.js        the site map: window.SITE = {title, home, pages:[{t, h, c:[...]}]}
└── <folder>/<page>.html  each page links ../assets/... with a relative path
```

Keep the site in **its own folder** (`docs/`), apart from source code and
run outputs. **Folder and page file names are ASCII, lower case, `_` between
words** (`ly_thuyet/steps/step_05_virtual_memory_paging.html`); the visible
titles (h1, `t` in `site.js`, breadcrumbs) keep the real text with accents.

Each page sets `<body data-root="../../" data-page="Folder/Page.html">`:
`data-root` is the path back to the site root, `data-page` the page's own path
from the root (it must equal its `h` in `site.js`, which is how the sidebar
marks "you are here" and builds prev/next). Everything is relative, so the
site works from `file://` with no server. **When adding a page, add it to
`site.js` too.** A folder node may have its own page (`h`) plus children (`c`).
When the theme files in the skill change, copy them over the site's `assets/`.

## Page anatomy

```html
<header class="top"><div class="top-inner">
  <div class="crumbs">Site<span class="sep">/</span>Folder</div>
  <h1>Title</h1>
  <p class="summary">Optional one-paragraph summary.</p>
  <div class="facts"><span class="fact done"><b>Status</b> done</span><span class="fact"><b>Tags</b> a, b</span></div>
</div></header>
<div class="layout">
  <aside class="side"></aside>          <!-- filled by noir.js -->
  <main> …content… <nav class="pager"></nav><footer>…</footer></main>
</div>
```

Content components (all shown in `page.html`):

- `h2` sections, `h3` subsections, each with an `id` (the page contents list
  is built from `h2[id]`, `h3[id]`).
- `h2.card-head` / `h3.card-head`: the inverted paper title card. Use it for
  the **one** section a reader should start with (e.g. the plain-language
  summary). Not for every heading.
- `p.lede`: the italic one-liner that opens a section in plain words.
- `.code > .code-bar(lang) + pre > code`: code as **plain escaped text**
  (`&lt;` `&gt;` `&amp;`), no spans; `noir.js` colours it by the first word of
  the bar. Languages: `c`, `bash`, `asm`, `ld`, `python`, `make`,
  `powershell`, `ini`, `json` (+ aliases `sh`, `nasm`, `h`, `py`, `mk`, …).
  Anything else (`text`, `output`) stays plain. A `tree` block may carry
  hand-made `.d` / `.n` spans; blocks that already contain spans are left
  alone. To add a language, add an entry to `RULES` in `noir.js`; its regexes
  must not contain capturing groups (use `(?:…)`).
- `.table-wrap > table`: wide tables scroll inside the wrapper. `td.nowrap` to
  keep a cell on one line.
- `.callout.note|warn|danger|quote` with an optional `.label` first child.
  One or two per section at most; `danger` is the only red on the page.
- `figure.diagram > svg` for diagrams, **inline** (+ optional `figcaption`):
  no extra request, works in any preview, text stays selectable. With more
  than one SVG on a page, give each root a unique `id`, prefix every inner id
  (`svg-x-arrow`, and its `url(#…)` / `href="#…"` users) and scope its
  `<style>` rules under `#svg-x`; all diagrams tend to reuse `id="arrow"` and
  class names like `.lbl`. The page is dark-only, so apply the SVG's
  `prefers-color-scheme: dark` rules unconditionally and drop the light ones —
  inside an inline SVG they follow the OS theme, not the page.
  `figure.diagram > img` is for raster images.
- `.pair > .card` ×2 for side-by-side comparisons; `kbd` for keys; `mark`.

## Style rules (hard)

- **Noir, dark only.** `color-scheme: dark`, no light theme unless asked.
  Page `#0b0b0b`, panels `#121212`, code `#000`, lines `#242424`/`#3a3a3a`,
  text `#cfcfcf`, headings/links `#fff`, muted `#858585`, grey `#a6a6a6`,
  paper `#f0f0f0`. **Only colours:** danger red (`#c62828` / `#ff6b6b`) and
  the syntax colours in code blocks.
- **Square corners.** Keep `*, *::before, *::after { border-radius: 0 !important; }`.
- **Type: IBM Plex Mono for everything** (body 14.5px/1.75, code 13px),
  bundled in `assets/fonts/` (SIL OFL, so it may live in a public repo) and
  declared in `noir.css` per script (`latin`, `latin-ext`, `vietnamese`;
  weights 400, 400 italic, 600, 700) with `unicode-range`. Labels, h1, table
  headers, sidebar groups and code bars are the same face in 600/700,
  uppercase, letter-spaced .08–.12em. Glyphs outside the subsets (box drawing
  `├──`) fall back to Cascadia/Consolas. The user asked for Berkeley Mono
  first: it is commercial and its licence forbids redistribution, so do not
  bundle it; Plex Mono was the chosen free stand-in.
- **Offline.** No CDN, nothing fetched: fonts are local files next to the CSS.
- **Mobile (≤ 900px).** One column, sidebar above the text, 16px gutters, no
  horizontal page scroll. `main` has `overflow-wrap: break-word` because long
  `A/B/C/D` runs once overflowed a 375px screen.
- Diagrams are SVG, not ASCII art, and their text is English (see the
  `dark-native-diagrams` skill for drawing them).

## Writing rules

- Page language follows the project (a Vietnamese vault stays Vietnamese);
  theme chrome ("On this page", "Copy") stays English.
- Short sentences; each fact once, link to it elsewhere.
- Commands copy-pasteable; admin-only ones in their own block
  (`powershell · admin`, `bash · sudo`).
- Only write what was verified; say plainly what is untested.

## Converting an Obsidian vault

```bash
py -3 -m pip install markdown-it-py beautifulsoup4
py -3 <skill>/scripts/obsidian_to_noir.py <vault> --out <vault>/docs --site-title "My notes" --lang vi --order "A,B,C"
```

It writes the site into `--out` (default `<vault>/docs`) with ASCII
lower-case names (`Lý thuyết/Step 05 - Paging.md` →
`ly_thuyet/step_05_paging.html`, `Home.md` → `index.html`), copies
`noir.css`/`noir.js` to `<out>/assets/`, writes `<out>/assets/site.js`, and
prints every unresolved link, missing heading or name clash. `--order` takes
the original folder/page names. What it maps:

| Obsidian | HTML |
| --- | --- |
| `[[Note]]`, `[[Note\|alias]]`, `[[Note#Heading]]` | relative link (heading → slug id, checked) |
| `![[x.svg\|800]]` | `figure.diagram > svg`, pasted inline (ids prefixed, CSS scoped, dark rules forced) |
| `![[x.png]]` | `figure.diagram > img` |
| folder `Lý thuyết/` + note `Lý thuyết.md` inside | sidebar group "Lý thuyết" whose heading is that page (`ly_thuyet/ly_thuyet.html`) |
| `> [!warning] Title` | `.callout.warn` with label (note/warn/danger) |
| `> _one italic line_` right after a heading | `p.lede` |
| `> ⚠️ …` | `.callout.warn`; other quotes → `.callout.quote` |
| `## Nói thật đơn giản` / `In plain words` | `h2.card-head` |
| front matter `status`, `tags` | fact chips in the header |
| emoji | dropped; ✅/❌ become ✓/✗ |
| hard-wrapped lines | reflowed; a line starting `**Label:**` or an arrow keeps its break |

`CLAUDE.md`, `.claude/`, `.obsidian/`, `*/src/*` are skipped (add more with
`--exclude`); links to a skipped `.md` point at the file itself (e.g.
`../CLAUDE.md`). Paths written as text in the notes (`Folder/src/…`) are not
rewritten — fix them in the Markdown before converting if files move.
Nothing is deleted: check the pages first, then remove the `.md` files and
the SVG folder (Recycle Bin or `git rm`), since after that the HTML is the
source and is edited by hand.
ASCII diagrams inside plain code fences stay as `text` blocks; redraw them as
SVG afterwards.

## Verify in a browser

Serve the folder (`py -3 -m http.server`) — a `file://` preview in Claude's
browser pane is a static snapshot without the linked CSS/JS. Set the viewport
size explicitly (a hidden pane reports width 0), then per page, at 1400px and
375px:

```js
const d = document.documentElement;
({ overflow: d.scrollWidth - d.clientWidth,                         // 0
   badAnchors: [...document.querySelectorAll('a[href^="#"]')]
     .map(a => decodeURIComponent(a.getAttribute('href').slice(1)))
     .filter(id => id && !document.getElementById(id)),              // []
   rounded: [...document.querySelectorAll('*')]
     .filter(e => getComputedStyle(e).borderRadius !== '0px').length,  // 0
   copy: document.querySelectorAll('.code-bar button').length
       === document.querySelectorAll('.code').length,                 // true
   brokenImg: [...document.querySelectorAll('img')].filter(i => !i.naturalWidth).length, // 0
   broken: document.querySelectorAll('a.broken').length })             // 0
```

For a whole site, load each page of `site.js` into a hidden `<iframe>` of the
wanted width and run the same checks (popups are blocked). When `overflow` is
not 0, find the element whose right edge passes the viewport and that has no
`overflow-x: auto` ancestor — usually a long unbreakable string.

Then look at a few pages by eye (a code-heavy one, one with diagrams, one with
tables) before reporting where the files are and how to open them.
