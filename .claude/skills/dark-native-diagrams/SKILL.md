---
name: dark-native-diagrams
description: >-
  Convert ASCII / box-drawing diagrams in this Obsidian vault into clean,
  dark-native, theme-adaptive SVGs in the house style, and author new SVG
  diagrams for notes the same way. Use this whenever a note contains an ASCII
  diagram (│ ┌ ┐ └ ┘ ├── ▼ ▲ → arrows, memory maps, swimlanes, flow charts,
  layer stacks, trees) that should become an SVG; when the user says "convert
  the diagram", "make/create an SVG", "vẽ sơ đồ", "tạo svg", "redraw / vẽ lại",
  or asks to fix a diagram's colors or style; when adding a diagram to a note;
  or when an existing SVG needs restyling to match the vault (dark boxes, light
  text, adaptive light/dark). Prefer this skill over ad-hoc SVG writing so every
  diagram in the vault stays visually consistent.
---

# Dark-native diagrams

The vault renders in both light and dark Obsidian themes, so every diagram must
look right in **both** without a hard-coded page background. The house style is
therefore **dark-native + theme-adaptive**: boxes have a dark fill with a light
stroke and light text (readable on a dark page), and a `@media (prefers-color-scheme)`
block nudges the neutral line/label colors so nothing washes out on a light page.
Arrowheads and lines inherit the line color via `context-stroke` so you never
restate a color twice.

Follow this style for **every** diagram — converting an ASCII block, drawing a
new one, or restyling an old export — so the whole vault reads as one system.

## When you're converting vs. keeping

Convert to SVG only **real diagrams**: box/flow charts, memory or register maps,
swimlanes, layer stacks, trees of boxes, sequence/interleaving diagrams.

**Keep as-is** (do NOT convert): ```code fences (```c, ```dts, ```python…),
markdown tables, and **directory/file trees** drawn with `├── └──` — those read
better as monospace text. When a note mixes several blocks, convert the diagrams
and leave the code/tables/trees untouched.

## The boilerplate (copy this)

Every SVG starts from this skeleton. Fill in boxes and arrows; keep the marker
and the `<style>` block verbatim.

```svg
<svg width="100%" viewBox="0 0 W H" xmlns="http://www.w3.org/2000/svg" font-family="-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif">
<defs>
  <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
    <path d="M2 1L8 5L2 9" fill="none" stroke="context-stroke" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  </marker>
  <style>.lbl{fill:#5c5a52;} .mono{font-family:'SF Mono',Consolas,monospace;} .tline{stroke:#c7c4ba;} .flow{ stroke:#8C8A82; }
  @media (prefers-color-scheme: dark){ .lbl{fill:#c2c0b6;} .tline{stroke:rgba(222,220,209,0.3);} .flow{stroke:#7d7b73;} }</style>
</defs>

<!-- a box -->
<rect x="130" y="34" width="260" height="48" fill="#0C447C" stroke="#85B7EB" stroke-width="1"/>
<text x="260" y="55" text-anchor="middle" fill="#B5D4F4" font-size="11.5" font-weight="600">Title</text>
<text x="260" y="72" text-anchor="middle" fill="#85B7EB" font-size="9" class="mono">detail · mono line</text>

<!-- an arrow (marker matches the line color via context-stroke) -->
<line x1="260" y1="82" x2="260" y2="115" class="flow" stroke-width="1.4" marker-end="url(#arrow)"/>

<!-- a caption / footnote -->
<text x="260" y="150" text-anchor="middle" class="lbl" font-size="9">one-line takeaway</text>
</svg>
```

- `.flow` = flow arrows/lines. `.lbl` = captions & labels. `.tline` = a boundary
  rule (dashed swimlane divider). `.mono` = code/hex/addresses.
- `class` handles the theme-adaptive neutrals; box colors are inline (dark fill
  never needs to change between themes because the text on it is light).

## Palette

Each box = **dark fill / light stroke / light text**, all inline. Use the title
color for the bold title and the mid (stroke) color for sub-lines.

| Role | fill | stroke | title text | sub text (mid) |
|---|---|---|---|---|
| blue | `#0C447C` | `#85B7EB` | `#B5D4F4` | `#85B7EB` |
| green | `#2C4E33` | `#8FC79A` | `#C6E7CC` | `#8FC79A` |
| amber | `#5C3F0E` | `#D8A54A` | `#F0DBAE` | `#D8A54A` |
| purple | `#3C3489` | `#AFA9EC` | `#CECBF6` | `#AFA9EC` |
| neutral | `#444441` | `#B4B2A9` | `#D3D1C7` | `#B4B2A9` |
| red (danger) | `#5A2626` | `#D98A8A` | `#F0C9C9` | `#EEC2C2` |

Neutral line/label classes (in the boilerplate): `.flow` `#8C8A82`→dark `#7d7b73`;
`.lbl` `#5c5a52`→`#c2c0b6`; `.tline` `#c7c4ba`→`rgba(222,220,209,0.3)`.

Use color to carry meaning (e.g. one subsystem = one color), not decoration.
Reuse the same palette color for a parent box and its children in a tree.

## Layout rules — why they matter

These come from real corrections; each prevents a specific ugly failure.

- **Boxes are sharp rectangles** — square corners, no `rx` rounding. Every `<rect>` is a plain rectangle; this is a deliberate house choice.
- **Arrows are horizontal or vertical only** — never diagonal. When two boxes
  aren't aligned, route the arrow as an elbow (a vertical segment + a horizontal
  segment), not a slanted line. Diagonal arrows read as sloppy and cross other
  content.
- **No text on top of a line.** A label must sit beside its arrow, not over it.
  If a label has to sit *along* a vertical arrow, break the arrow into two
  segments with the text in the gap, or offset the text sideways.
  - **Axis-label gotcha:** a rotated axis label put at the same x as the axis
    line lands *on* the line. Offset them: draw the line at e.g. `x=24` and
    rotate the text around `x=50` — `<text transform="rotate(90 50 190)" x="50" …>`.
- **No text overflowing a box.** Size the box to the text, or shrink the font.
  For a centered label, remember it grows both directions from center. Long
  detail belongs in a footnote under the diagram, not crammed inside a box.
- **Arrows must be semantically true.** A many-to-many relationship is not N
  parallel 1:1 arrows — route them through a small collector bar (fan-in to one
  node, then one arrow out), or the reader infers a pairing that isn't there.
  Read direction matters too: a read pulls *from* the store (arrow points to the
  reader), a write pushes *to* it.
- **Cross-lane arrows** start/end at a box's edge at the box's **vertical
  center**, so parallel arrows look symmetric rather than "lệch".
- **Diagram text is English** (titles, labels, captions), even when the note is
  Vietnamese — matching the vault's diagram convention. Keep genuinely
  proper/Sino-Vietnamese concept names if that's the subject (rare).

## Saving and embedding

1. Save to `Diagram/<Note Title>/<name>.svg`. Obsidian resolves embeds by
   **filename alone**, so the folder is just organization — but that means
   filenames must be unique across the vault. If an older copy of the same image
   exists elsewhere (e.g. under `Image/`), delete it so the name stays unique.
2. Embed in the note by filename: `![[<name>.svg]]`.
3. **Preserve every web link.** Before editing a note, count its `http`/`https`
   occurrences; after, count again — they must match. The vault owner does not
   tolerate silent link loss. When you replace a fenced ASCII block with an
   embed, only the diagram text goes away; any links in the note stay put.

### Replacing a fenced ASCII block with the embed

Use a Python pass so you don't disturb code fences or links. Match the specific
fenced block and swap it for the embed:

```python
import io, re
f = "Zettelkasten/<Note>.md"
t = io.open(f, encoding="utf-8").read(); before = t.count("http")
# ```text fences are safe to match wholesale:
t2 = re.sub(r"```text\n.*?\n```", "![[<name>.svg]]", t, flags=re.DOTALL)
# For a plain ``` fence, anchor on a unique phrase so it can't span a ```c block:
# t2 = re.sub(r"```\n[^`]*?<UNIQUE ANCHOR>[^`]*?\n```", "![[<name>.svg]]", t, flags=re.DOTALL)
io.open(f, "w", encoding="utf-8", newline="").write(t2)
assert before == t2.count("http"), "link count changed!"
```

Run Python via `py -3` with `PYTHONIOENCODING=utf-8`; write files with
`io.open(..., encoding="utf-8", newline="")`. When a script has tricky quotes,
write it to a scratch `.py` file and run it rather than fighting a heredoc.

## Common shapes (quick reference)

- **Flow / pipeline:** a column (or row) of boxes joined by `.flow` arrows; put
  the step verb as a small `.lbl` beside each arrow.
- **Layer stack:** full-width bands top→bottom, left-aligned title + one gloss
  line each, distinct palette color per layer, a left axis arrow for the
  gradient (abstraction, scope…). See the axis-label gotcha.
- **Swimlanes / sequence:** vertical dashed `.tline` lifelines per actor, boxes
  on each lane at time rows, horizontal arrows between lanes labeled read/write;
  a red result box at the bottom for the punchline.
- **Tree / hierarchy:** a root box, a horizontal bus, vertical drop arrows to
  children; children share the parent's color family.

After writing an SVG, sanity-check it mentally against the rules above (arrows
orthogonal? text clear of lines? nothing overflowing? links preserved?) before
telling the user it's done.
