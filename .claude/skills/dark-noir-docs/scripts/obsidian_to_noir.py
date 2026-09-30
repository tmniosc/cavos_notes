#!/usr/bin/env python3
"""Convert an Obsidian vault of Markdown notes into dark-noir-docs HTML pages.

The site goes into its own folder (default <vault>/docs). Page and folder
names there are ASCII, lower case, _ between words ("Lý thuyết/Step 05 -
Paging.md" -> "ly_thuyet/step_05_paging.html"); titles keep the original text.
Home.md becomes index.html. The theme files are copied to <out>/assets/ and
the site map is written to <out>/assets/site.js. SVG embeds are pasted inline.
Nothing is deleted: remove the .md files yourself once the pages are checked.

    py -3 obsidian_to_noir.py <vault> [--out DIR] [--site-title T] [--exclude GLOB ...]
                              [--order "A,B,C"] [--lang vi] [--dry-run]

Needs: markdown-it-py, beautifulsoup4.
"""
import argparse
import fnmatch
import html
import json
import os
import re
import shutil
import sys
import unicodedata
from pathlib import Path, PurePosixPath
from urllib.parse import quote

from bs4 import BeautifulSoup
from bs4.element import NavigableString
from markdown_it import MarkdownIt

HERE = Path(__file__).resolve().parent
ASSETS = HERE.parent / 'assets'
MEDIA = {'.svg', '.png', '.jpg', '.jpeg', '.gif', '.webp', '.pdf'}
DEFAULT_EXCLUDE = ['CLAUDE.md', '.claude/*', '.obsidian/*', '.git/*', '.trash/*', '*/src/*', 'assets/*']
PLAIN_HEADINGS = ('nói thật đơn giản', 'in plain words', 'the short version')

# --------------------------------------------------------------- characters
# Emoji are dropped (house rule: no icons). The two that carry meaning in
# tables become text ticks; a warning sign at the start of a quote turns the
# quote into a warn callout (see WARN).
WARN = '\ue001WARN\ue001'
MARK = '\ue000'   # callout marker; markdown-it turns NUL into U+FFFD, so use private-use chars
REPLACE = {'✅': '✓', '✔': '✓', '❌': '✗', '✖': '✗'}
EMOJI = re.compile('[\U0001F000-\U0001FAFF☀-⛿✀-✒✘-➿⬀-⯿'
                   '⌚-⏿️‍]')


def clean_text(md):
    md = re.sub(r'^(\s*>\s*)⚠️?\s*', lambda m: m.group(1) + WARN, md, flags=re.M)
    for k, v in REPLACE.items():
        md = md.replace(k, v)
    md = EMOJI.sub('', md)
    # "## 🎯 Mục đích" leaves a double space after the emoji is gone
    md = re.sub(r'^(#{1,6}) +', r'\1 ', md, flags=re.M)
    return md


# --------------------------------------------------------------- inline SVG
# SVGs are pasted into the page rather than linked, so several on one page
# share one document: every id gets a per-diagram prefix, and every CSS rule
# is scoped to that diagram's root. The page is dark-only while the files are
# theme-adaptive, so "prefers-color-scheme: dark" rules are applied always and
# "light" ones dropped (otherwise a light OS would paint dark text on black).
def _css_blocks(css):
    """Yield (prelude, body) for each top-level block of a stylesheet."""
    i = 0
    while True:
        j = css.find('{', i)
        if j < 0:
            return
        depth, k = 1, j + 1
        while k < len(css) and depth:
            depth += {'{': 1, '}': -1}.get(css[k], 0)
            k += 1
        yield css[i:j].strip(), css[j + 1:k - 1]
        i = k


def scope_css(css, root):
    css = re.sub(r'/\*.*?\*/', '', css, flags=re.S)
    base, dark = [], []
    for prelude, body in _css_blocks(css):
        if prelude.startswith('@media'):
            if 'prefers-color-scheme' in prelude:
                if 'dark' in prelude:
                    dark.append(scope_css(body, root))
                continue
            base.append(f'{prelude}{{{scope_css(body, root)}}}')
        elif prelude.startswith('@'):
            base.append(f'{prelude}{{{body}}}')
        else:
            sels = ', '.join(s if s.strip().startswith(root) else f'{root} {s.strip()}'
                             for s in prelude.split(',') if s.strip())
            base.append(f'{sels}{{{body.strip()}}}')
    return ' '.join(base + dark)


def inline_svg(text, prefix, alt, width=None):
    text = re.sub(r'<\?xml[^>]*\?>|<!DOCTYPE[^>]*>', '', text).strip()
    ids = set(re.findall(r'\bid="([^"]+)"', text))
    for i in sorted(ids, key=len, reverse=True):
        text = re.sub(rf'\bid="{re.escape(i)}"', f'id="{prefix}-{i}"', text)
        text = re.sub(rf'url\(\s*#{re.escape(i)}\s*\)', f'url(#{prefix}-{i})', text)
        text = re.sub(rf'(href="#){re.escape(i)}"', rf'\g<1>{prefix}-{i}"', text)
    text = re.sub(r'(<style[^>]*>)(.*?)(</style>)',
                  lambda m: m.group(1) + scope_css(m.group(2), f'#{prefix}') + m.group(3), text, flags=re.S)
    extra = f' id="{prefix}" role="img" aria-label="{html.escape(alt)}"'
    if width:
        extra += f' style="max-width:{width}px"'
    text = re.sub(r'<svg\b', '<svg' + extra, text, count=1)
    return re.sub(r'\n\s*\n', '\n', text)


def folder_title(name):
    """A folder is shown with spaces even if it uses _ on disk. Its index page is
    the note named like the folder (Lý thuyết/Lý thuyết.md)."""
    return name.replace('_', ' ')


def ascii_name(name):
    """Output file/folder name: no diacritics, lower case, _ between words.
    'Step 05 - Virtual Memory & Paging' -> 'step_05_virtual_memory_paging'."""
    name = unicodedata.normalize('NFD', name.replace('đ', 'd').replace('Đ', 'D'))
    name = ''.join(c for c in name if not unicodedata.combining(c)).lower()
    return re.sub(r'[^a-z0-9]+', '_', name).strip('_') or 'page'


def slug(text):
    text = unicodedata.normalize('NFC', text).strip().lower()
    text = re.sub(r'[^\w\s-]', '', text)
    return re.sub(r'[\s_]+', '-', text).strip('-') or 'section'


def front_matter(src):
    meta = {}
    if src.startswith('---\n'):
        end = src.find('\n---', 4)
        if end > 0:
            for line in src[4:end].splitlines():
                if ':' in line:
                    k, v = line.split(':', 1)
                    v = v.strip()
                    if v.startswith('[') and v.endswith(']'):
                        v = [x.strip().strip('"\'') for x in v[1:-1].split(',') if x.strip()]
                    meta[k.strip()] = v
            src = src[end + 4:].lstrip('\n')
    return meta, src


# ------------------------------------------------------------------- vault
class Vault:
    def __init__(self, root, exclude, home, out):
        self.root = root
        self.out = out
        self.notes = {}      # stem -> PurePosixPath of .md (relative)
        self.media = {}      # file name -> PurePosixPath (relative)
        self.kept_md = {}    # stem -> excluded .md that stays Markdown (e.g. CLAUDE.md)
        self.home = home
        for p in sorted(root.rglob('*')):
            if not p.is_file():
                continue
            rel = PurePosixPath(p.relative_to(root).as_posix())
            if any(fnmatch.fnmatch(str(rel), g) for g in exclude):
                if p.suffix == '.md' and not rel.parts[0].startswith('.'):
                    self.kept_md.setdefault(p.stem, rel)   # linked to as a plain .md file
                continue
            if p.suffix == '.md':
                if p.stem in self.notes:
                    print(f'warning: duplicate note name {p.stem}: {rel} and {self.notes[p.stem]}', file=sys.stderr)
                self.notes[p.stem] = rel
            elif p.suffix.lower() in MEDIA:
                self.media[p.name] = rel

    def html_path(self, stem):
        """Path of the page inside the output folder."""
        if stem == self.home:
            return PurePosixPath('index.html')
        rel = self.notes[stem]
        return PurePosixPath(*[ascii_name(p) for p in rel.parent.parts], ascii_name(stem) + '.html')

    def outside(self, rel):
        """A vault file that is not copied into the output (CLAUDE.md, an image), as a
        path relative to the output root, e.g. ../CLAUDE.md."""
        return PurePosixPath(os.path.relpath(self.root / rel, self.out).replace(os.sep, '/'))


def rel_href(from_page, target):
    """Relative URL from page (a PurePosixPath) to target (PurePosixPath)."""
    a = list(from_page.parent.parts)
    b = list(target.parts)
    while a and b and a[0] == b[0]:
        a.pop(0)
        b.pop(0)
    return quote('/'.join(['..'] * len(a) + b), safe='/')


# ------------------------------------------------------------- markdown-it
def wikilink_plugin(md, ctx):
    def rule(state, silent):
        s, pos = state.src, state.pos
        embed = s.startswith('![[', pos)
        if not (embed or s.startswith('[[', pos)):
            return False
        start = pos + (3 if embed else 2)
        end = s.find(']]', start)
        if end < 0 or '\n' in s[start:end]:
            return False
        if not silent:
            inner = s[start:end].replace('\\|', '|')
            tok = state.push('html_inline', '', 0)
            tok.content = ctx.render_link(inner, embed)
        state.pos = end + 2
        return True
    md.inline.ruler.before('link', 'wikilink', rule)

    # Obsidian shows every newline as a line break, and these notes are
    # hard-wrapped at ~110 columns. Reflow the text, but keep a break before a
    # line that starts a new "**Label:**" row or an arrow line.
    def softbreak(self, tokens, idx, options, env):
        rest = [t for t in tokens[idx + 1:idx + 7] if not (t.type == 'text' and not t.content)]
        if rest:
            if rest[0].type == 'text' and re.match(r'\s*(?:→|←|⇒|=>|->)', rest[0].content):
                return '<br>\n'
            if rest[0].type == 'strong_open' and len(rest) > 2:
                inner, after = rest[1], rest[3] if len(rest) > 3 else None
                if inner.content.rstrip().endswith(':') or (after is not None and after.type == 'text'
                                                            and after.content.startswith(':')):
                    return '<br>\n'
        return '\n'
    md.add_render_rule('softbreak', softbreak)


class Converter:
    def __init__(self, vault, site_title, lang):
        self.v = vault
        self.site_title = site_title
        self.lang = lang
        self.md = MarkdownIt('commonmark', {'html': True, 'typographer': False}).enable(['table', 'strikethrough'])
        wikilink_plugin(self.md, self)
        self.page = None
        self.problems = []
        self.anchor_refs = []   # (from page, target html path, anchor)
        self.ids = {}           # html path -> set of ids

    def render_link(self, inner, embed):
        target, _, alias = inner.partition('|')
        name, _, anchor = target.partition('#')
        name = name.strip()
        anchor = anchor.strip()
        if embed and PurePosixPath(name).suffix.lower() in MEDIA:
            rel = self.v.media.get(PurePosixPath(name).name)
            if rel is None:
                self.problems.append(f'{self.page}: missing file {name}')
                return f'<span class="broken">[missing: {html.escape(name)}]</span>'
            width = alias.strip() if alias.strip().isdigit() else None
            alt = PurePosixPath(name).stem.replace('-', ' ')
            if rel.suffix.lower() == '.svg':
                # pasted in after BeautifulSoup is done: html.parser would
                # lowercase viewBox, refX, markerWidth...
                n = len(self.svgs)
                prefix = 'svg-' + slug(PurePosixPath(name).stem)
                if any(p == prefix for p, _ in self.svgs):
                    prefix += f'-{n}'
                svg = (self.v.root / rel).read_text(encoding='utf-8')
                self.svgs.append((prefix, inline_svg(svg, prefix, alt, width)))
                return f'<figure class="diagram" data-svg-slot="{n}"></figure>'
            style = f' style="width:{width}px"' if width else ''
            alt = html.escape(alt)
            return (f'<figure class="diagram"><img src="{rel_href(self.page, self.v.outside(rel))}" alt="{alt}"{style}>'
                    f'</figure>')
        label = html.escape(alias.strip() or (f'{name} › {anchor}' if name and anchor else name or anchor))
        if not name:                               # [[#Heading]] on the same page
            return f'<a href="#{quote(slug(anchor))}">{label}</a>'
        stem = PurePosixPath(name).name
        if stem.endswith('.md'):
            stem = stem[:-3]
        if stem not in self.v.notes:
            if stem in self.v.kept_md:
                return f'<a href="{rel_href(self.page, self.v.outside(self.v.kept_md[stem]))}">{label}</a>'
            if PurePosixPath(stem).suffix.lower() in MEDIA and stem in self.v.media:
                return f'<a href="{rel_href(self.page, self.v.outside(self.v.media[stem]))}">{label}</a>'
            self.problems.append(f'{self.page}: unresolved link [[{inner}]]')
            return f'<a class="broken" title="No page named {html.escape(stem)}">{label}</a>'
        dest = self.v.html_path(stem)
        href = rel_href(self.page, dest) if dest != self.page else ''
        if anchor:
            a = slug(anchor)
            self.anchor_refs.append((self.page, dest, a, anchor))
            href += '#' + quote(a)
        return f'<a href="{href or "#"}">{label}</a>'

    # ---------------------------------------------------------- one note
    def convert(self, stem):
        src_path = self.v.root / self.v.notes[stem]
        self.page = self.v.html_path(stem)
        self.svgs = []
        meta, body = front_matter(src_path.read_text(encoding='utf-8'))
        body = clean_text(body.replace('\r\n', '\n'))
        body = self._callouts(body)
        title = html.escape(stem)
        m = re.match(r'\s*#\s+(.+?)\s*\n', body)
        if m:
            title = self.md.renderInline(m.group(1).strip())   # titles may hold `code`
            body = body[m.end():]
        soup = BeautifulSoup(self.md.render(body), 'html.parser')
        self._post(soup)
        self.ids[self.page] = {str(t['id']) for t in soup.find_all(id=True)}
        return self._page(stem, title, meta, soup)

    @staticmethod
    def _callouts(md):
        # "> [!warning] Title" -> a marker paragraph the post-processor turns
        # into the callout label.
        def repl(m):
            return f'{m.group(1)}{MARK}CALLOUT:{m.group(2).lower()}:{m.group(3).strip()}{MARK}\n{m.group(1).rstrip()}\n'
        return re.sub(r'^(\s*>\s*)\[!(\w+)\][+-]?[ \t]*([^\n]*)\n', repl, md, flags=re.M)

    def _post(self, soup):
        # code blocks -> .code > .code-bar + pre > code
        for pre in soup.find_all('pre'):
            code = pre.find('code')
            lang = 'text'
            if code is not None:
                for c in code.get('class', []):
                    if c.startswith('language-'):
                        lang = c[9:] or 'text'
                code.attrs.pop('class', None)
            wrap = soup.new_tag('div', attrs={'class': 'code'})
            bar = soup.new_tag('div', attrs={'class': 'code-bar'})
            bar.string = lang
            pre.wrap(wrap)
            pre.insert_before(bar)
            # markdown-it leaves the final newline inside <code>
            if code is not None and code.contents and isinstance(code.contents[-1], NavigableString):
                code.contents[-1].replace_with(code.contents[-1].rstrip('\n'))
        # tables scroll on their own
        for t in soup.find_all('table'):
            t.wrap(soup.new_tag('div', attrs={'class': 'table-wrap'}))
        # a paragraph that only holds a figure
        for fig in soup.find_all('figure'):
            p = fig.parent
            if p is not None and p.name == 'p' and not p.get_text(strip=True):
                p.unwrap()
        # headings: stray h1 -> h2, ids, the plain-language title card
        used = set()
        for h in soup.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6']):
            if h.name == 'h1':
                h.name = 'h2'
            base = slug(h.get_text())
            hid, n = base, 2
            while hid in used:
                hid, n = f'{base}-{n}', n + 1
            used.add(hid)
            h['id'] = hid
            if h.name in ('h2', 'h3') and h.get_text(strip=True).lower().startswith(PLAIN_HEADINGS):
                h['class'] = ['card-head']
        # blockquotes
        for bq in soup.find_all('blockquote'):
            self._quote(soup, bq)

    @staticmethod
    def _quote(soup, bq):
        first = bq.find(['p', 'div', 'ul', 'ol', 'pre'], recursive=False)
        text = bq.get_text()
        # "> [!type] Title"
        if first is not None and first.name == 'p' and first.get_text().startswith(MARK + 'CALLOUT:'):
            _, kind, label = first.get_text().strip().strip(MARK).split(':', 2)
            kind = {'warning': 'warn', 'caution': 'warn', 'attention': 'warn', 'danger': 'danger',
                    'error': 'danger', 'bug': 'danger', 'failure': 'danger'}.get(kind, 'note')
            lab = soup.new_tag('div', attrs={'class': 'label'})
            lab.string = label.strip(MARK).strip() or kind
            first.replace_with(lab)
            bq.name = 'div'
            bq['class'] = ['callout', kind]
            return
        # the italic one-liner that opens a section
        prev = bq.find_previous_sibling()
        kids = [c for c in bq.children if not (isinstance(c, NavigableString) and not c.strip())]
        if (prev is not None and prev.name in ('h2', 'h3', 'h4') and len(kids) == 1 and kids[0].name == 'p'):
            inner = [c for c in kids[0].children if not (isinstance(c, NavigableString) and not c.strip())]
            if len(inner) == 1 and getattr(inner[0], 'name', None) == 'em':
                p = kids[0]
                p['class'] = ['lede']
                bq.replace_with(p)
                return
        bq.name = 'div'
        if WARN in text:
            for s in bq.find_all(string=lambda s: WARN in s):
                s.replace_with(s.replace(WARN, ''))
            bq['class'] = ['callout', 'warn']
        else:
            bq['class'] = ['callout', 'quote']

    def _page(self, stem, title, meta, soup):
        page = self.page
        root = '../' * (len(page.parts) - 1)
        crumbs = [f'<a href="{rel_href(page, PurePosixPath("index.html"))}">{html.escape(self.site_title)}</a>']
        parts = self.v.notes[stem].parent.parts if stem != self.v.home else ()
        for folder in map(folder_title, parts):
            if folder in self.v.notes and self.v.html_path(folder) != page:
                crumbs.append(f'<a href="{rel_href(page, self.v.html_path(folder))}">{html.escape(folder)}</a>')
            else:
                crumbs.append(html.escape(folder))
        crumbs_html = '<span class="sep">/</span>'.join(crumbs)
        facts = []
        status = meta.get('status')
        if status:
            cls = ' done' if status == 'done' else ''
            facts.append(f'<span class="fact{cls}"><b>Status</b> {html.escape(str(status))}</span>')
        tags = meta.get('tags')
        if tags:
            tags = tags if isinstance(tags, list) else [tags]
            facts.append(f'<span class="fact"><b>Tags</b> {html.escape(", ".join(tags))}</span>')
        facts_html = f'\n    <div class="facts">{"".join(facts)}</div>' if facts else ''
        body = re.sub(r'<figure class="diagram" data-svg-slot="(\d+)"></figure>',
                      lambda m: f'<figure class="diagram">\n{self.svgs[int(m.group(1))][1]}\n</figure>',
                      str(soup).strip())
        return f'''<!doctype html>
<html lang="{self.lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(BeautifulSoup(title, 'html.parser').get_text())} · {html.escape(self.site_title)}</title>
<link rel="stylesheet" href="{root}assets/noir.css">
</head>
<body data-root="{root}" data-page="{html.escape(str(page))}">
<header class="top">
  <div class="top-inner">
    <div class="crumbs">{crumbs_html}</div>
    <h1>{title}</h1>{facts_html}
  </div>
</header>
<div class="layout">
<aside class="side"></aside>
<main>
{body}
<nav class="pager" aria-label="Pages"></nav>
<footer>{html.escape(self.site_title)} · {html.escape(stem)}</footer>
</main>
</div>
<script src="{root}assets/site.js"></script>
<script src="{root}assets/noir.js"></script>
</body>
</html>
'''

    # ------------------------------------------------------------ site map
    def site_js(self, order):
        rank = {name: i for i, name in enumerate(order)}

        def natural(s):
            return [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', s)]

        def key(item):
            name = folder_title(item[0])
            return (rank.get(name, len(rank)), natural(name))

        tree = {}
        for stem, rel in self.v.notes.items():
            if stem == self.v.home:
                continue
            node = tree
            for folder in rel.parent.parts:
                node = node.setdefault(folder, {})
            node[stem + '\u0000'] = str(self.v.html_path(stem))

        def build(node, folder_name=None):
            out = []
            folders = sorted(((k, v) for k, v in node.items() if isinstance(v, dict)), key=key)
            pages = sorted(((k[:-1], v) for k, v in node.items() if not isinstance(v, dict)), key=key)
            for name, sub in folders:
                title = folder_title(name)
                entry = {'t': title, 'c': build(sub, title)}
                if title in self.v.notes:
                    entry['h'] = str(self.v.html_path(title))
                out.append(entry)
            for name, href in pages:
                if name != folder_name:            # the folder's own index page is its heading
                    out.append({'t': name, 'h': href})
            return out

        site = {'title': self.site_title, 'pages': build(tree)}
        if self.v.home in self.v.notes:
            site['home'] = {'t': self.v.home, 'h': 'index.html'}
        return ('// Site map for noir.js: page titles and paths relative to the site root.\n'
                '// Regenerate with obsidian_to_noir.py, or edit by hand when adding a page.\n'
                'window.SITE = ' + json.dumps(site, ensure_ascii=False, indent=1) + ';\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('vault', type=Path)
    ap.add_argument('--site-title', default='Notes')
    ap.add_argument('--home', default='Home', help='note that becomes index.html')
    ap.add_argument('--exclude', nargs='*', default=[])
    ap.add_argument('--order', default='', help='comma-separated folder/page names to list first, in order')
    ap.add_argument('--lang', default='en')
    ap.add_argument('--out', type=Path, help='output folder (default: <vault>/docs)')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()

    root = a.vault.resolve()
    out = (a.out or root / 'docs').resolve()
    vault = Vault(root, DEFAULT_EXCLUDE + a.exclude + [str(out.relative_to(root).as_posix()) + '/*']
                  if out.is_relative_to(root) else DEFAULT_EXCLUDE + a.exclude, a.home, out)
    conv = Converter(vault, a.site_title, a.lang)
    pages = {}
    for stem in vault.notes:
        pages[vault.html_path(stem)] = conv.convert(stem)
    for src, dest, anchor, text in conv.anchor_refs:
        if anchor not in conv.ids.get(dest, set()):
            conv.problems.append(f'{src}: no heading "{text}" (#{anchor}) in {dest}')

    order = [x.strip() for x in a.order.split(',') if x.strip()]
    if not a.dry_run:
        (out / 'assets').mkdir(parents=True, exist_ok=True)
        for f in ('noir.css', 'noir.js'):
            shutil.copyfile(ASSETS / f, out / 'assets' / f)
        (out / 'assets' / 'site.js').write_text(conv.site_js(order), encoding='utf-8')
        for path, text in pages.items():
            (out / path).parent.mkdir(parents=True, exist_ok=True)
            (out / path).write_text(text, encoding='utf-8')
    names = {}
    for stem in vault.notes:
        names.setdefault(vault.html_path(stem), []).append(stem)
    for path, stems in names.items():
        if len(stems) > 1:
            print(f'  name clash: {", ".join(stems)} -> {path}')
    print(f'{len(pages)} pages{" (dry run)" if a.dry_run else ""}')
    for p in conv.problems:
        print('  ' + p)


if __name__ == '__main__':
    main()
