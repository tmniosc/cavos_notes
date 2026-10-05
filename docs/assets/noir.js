/* dark-noir-docs runtime: syntax colours, copy buttons, the sidebar (site map +
   contents of this page), scroll-spy and prev/next links.
   Works from file:// with no server: the site map comes from site.js, loaded
   as a plain <script> before this file, which sets window.SITE. */
(function () {
  'use strict';

  // ---------------------------------------------------------------- colours
  // The language is the first word of the code bar ("c", "bash · wsl").
  // Each rule is [class, regex]; the regexes must not contain capturing groups.
  var C_KW = 'if|else|for|while|do|return|struct|typedef|static|const|volatile|void|unsigned|signed|int|char|long|short|bool|' +
             'float|double|sizeof|asm|__asm__|extern|inline|enum|union|switch|case|break|continue|default|goto|' +
             'u?int(?:8|16|32|64)_t|size_t|uintptr_t|true|false|NULL|__attribute__';
  var RULES = {
    c: [
      ['c', /\/\/[^\n]*|\/\*[\s\S]*?\*\//],
      ['s', /"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)+'/],
      ['k', new RegExp('^[ \\t]*#[ \\t]*\\w+|\\b(?:' + C_KW + ')\\b')],
      ['f', /\b[A-Za-z_]\w*(?=\s*\()/],
      ['n', /\b0x[0-9a-fA-F_]+\b|\b\d+(?:\.\d+)?[uUlL]*\b/]
    ],
    bash: [
      ['c', /(?:^|(?<=\s))#[^\n]*/],
      ['s', /"(?:[^"\\]|\\.)*"|'[^'\n]*'/],
      ['v', /\$\{[^}\n]*\}|\$\(|\$[\w@#?*]+/],
      ['k', /(?<=^[ \t]*|[|;&(][ \t]*|\bsudo[ \t]+|\bthen[ \t]+|\bdo[ \t]+)[A-Za-z_.~\/][\w.\/~+-]*/],
      ['f', /(?<=\s)--?[A-Za-z][\w-]*/],
      ['n', /\b\d+\b/]
    ],
    asm: [
      ['c', /;[^\n]*|\/\/[^\n]*|(?:^|(?<=\s))#[^\n]*/],
      ['s', /"[^"\n]*"/],
      ['f', /^[ \t]*[\w.$]+:/],
      ['k', /(?<=^[ \t]*)\.?[a-z][a-z0-9]*\b/],
      ['v', /%?\b(?:[re]?[abcd]x|[re]?[sd]il?|[re]?[sb]pl?|r\d{1,2}[dwb]?|cr\d|[cdefgs]s|[abcd][lh]|rip|[xy]mm\d+)\b/],
      ['n', /\$?\b0x[0-9a-fA-F]+\b|\$?\b\d+\b/]
    ],
    ld: [
      ['c', /\/\*[\s\S]*?\*\//],
      ['k', /\b(?:ENTRY|SECTIONS|PHDRS|MEMORY|OUTPUT_FORMAT|OUTPUT_ARCH|KEEP|ALIGN|CONSTANT|MAXPAGESIZE|FLAGS|PROVIDE|SIZEOF|ADDR|PT_LOAD|PT_DYNAMIC|PT_NOTE)\b/],
      ['f', /(?<![\w)])\.[a-z_][\w.*]*/],
      ['n', /\b0x[0-9a-fA-F]+\b|\b\d+[KM]?\b/]
    ],
    python: [
      ['c', /#[^\n]*/],
      ['s', /[rbf]?"""[\s\S]*?"""|[rbf]?"(?:[^"\\\n]|\\.)*"|[rbf]?'(?:[^'\\\n]|\\.)*'/],
      ['k', /\b(?:def|class|return|if|elif|else|for|while|in|not|and|or|import|from|as|with|try|except|finally|raise|pass|break|continue|lambda|None|True|False|yield|global)\b/],
      ['f', /\b[A-Za-z_]\w*(?=\s*\()/],
      ['n', /\b0x[0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b/]
    ],
    make: [
      ['c', /#[^\n]*/],
      ['v', /\$\([^)\n]*\)|\$\{[^}\n]*\}|\$[@<^*?]/],
      ['k', /^[ \t]*(?:ifeq|ifneq|ifdef|ifndef|else|endif|include|-include|export|define|endef|\.PHONY)\b/],
      ['p', /^[\w.\-]+(?=[ \t]*(?:[:?+]?=))/],
      ['f', /^[\w.\/%\-$() ]+(?=:(?!=))/],
      ['n', /\b\d+\b/]
    ],
    powershell: [
      ['c', /#[^\n]*/],
      ['s', /"(?:[^"`]|`.)*"|'[^']*'/],
      ['v', /\$(?:\{[^}]*\}|[\w:]+)/],
      ['k', /\b[A-Z][a-z]+-[A-Z][A-Za-z]+\b|\b(?:if|else|elseif|foreach|function|param|return|exit)\b/],
      ['f', /(?:^|(?<=[\s(]))--?[A-Za-z][\w-]*/],
      ['n', /\b\d+(?:\.\d+)*\b/]
    ],
    ini: [
      ['c', /;[^\n]*/],
      ['k', /^[ \t]*\[[^\]\n]+\]/],
      ['s', /"[^"\n]*"/],
      ['p', /^[ \t]*[\w.\-\/]+(?=[ \t]*=)/],
      ['n', /\b\d+[A-Za-z]?\b/]
    ],
    json: [
      ['p', /"(?:[^"\\\n]|\\.)*"(?=\s*:)/],
      ['s', /"(?:[^"\\\n]|\\.)*"/],
      ['k', /\b(?:true|false|null)\b/],
      ['n', /-?\b\d+(?:\.\d+)?\b/]
    ]
  };
  var ALIAS = { h: 'c', cpp: 'c', 'c++': 'c', sh: 'bash', shell: 'bash', zsh: 'bash', nasm: 'asm', gas: 'asm', s: 'asm',
                x86asm: 'asm', lds: 'ld', linker: 'ld', py: 'python', makefile: 'make', mk: 'make', ps1: 'powershell',
                pwsh: 'powershell', conf: 'ini', toml: 'ini' };

  function esc(t) { return t.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  function colour(block) {
    var bar = block.querySelector('.code-bar');
    var code = block.querySelector('pre code');
    if (!bar || !code || code.querySelector('span')) return;   // hand-made spans (tree) stay
    var lang = (bar.firstChild && bar.firstChild.nodeValue || '').trim().split(/\s/)[0].toLowerCase();
    var rules = RULES[ALIAS[lang] || lang];
    if (!rules) return;
    var re = new RegExp(rules.map(function (r) { return '(' + r[1].source + ')'; }).join('|'), 'gm');
    var text = code.textContent, out = '', last = 0, m;
    while ((m = re.exec(text))) {
      if (!m[0]) { re.lastIndex++; continue; }
      var i = 1;
      while (m[i] === undefined) i++;
      out += esc(text.slice(last, m.index)) + '<span class="tok-' + rules[i - 1][0] + '">' + esc(m[0]) + '</span>';
      last = re.lastIndex;
    }
    code.innerHTML = out + esc(text.slice(last));
  }

  // ------------------------------------------------------------------- copy
  // The Clipboard API is missing outside secure contexts (file://), so fall
  // back to a hidden textarea.
  function copyText(text) {
    if (navigator.clipboard && window.isSecureContext) return navigator.clipboard.writeText(text);
    var ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    var ok = document.execCommand('copy');
    document.body.removeChild(ta);
    return ok ? Promise.resolve() : Promise.reject();
  }
  function addCopy(block) {
    var bar = block.querySelector('.code-bar');
    var pre = block.querySelector('pre');
    if (!bar || !pre || bar.querySelector('button')) return;
    var btn = document.createElement('button');
    btn.type = 'button';
    btn.textContent = 'Copy';
    btn.addEventListener('click', function () {
      copyText(pre.innerText).then(function () { btn.textContent = 'Copied'; },
                                   function () { btn.textContent = 'Select + Ctrl+C'; });
      setTimeout(function () { btn.textContent = 'Copy'; }, 1500);
    });
    bar.appendChild(btn);
  }

  document.querySelectorAll('.code').forEach(function (b) { colour(b); addCopy(b); });

  // ---------------------------------------------------------------- sidebar
  var side = document.querySelector('.side');
  var root = document.body.getAttribute('data-root') || '';
  var here = document.body.getAttribute('data-page') || '';
  var flat = [];                                   // pages in reading order, for prev/next

  function el(tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    return e;
  }
  function link(node) {
    var a = el('a', null, node.t);
    a.href = root + node.h;
    if (node.h === here) a.className = 'here';
    return a;
  }
  function contains(node) {
    if (node.h === here) return true;
    return (node.c || []).some(contains);
  }
  function tree(nodes, depth) {
    var ul = el('ul');
    nodes.forEach(function (n) {
      var li = el('li');
      if (n.c && n.c.length) {
        var d = el('details');
        if (contains(n) || (depth === 0 && !here)) d.open = true;
        var s = el('summary');
        if (n.h) { s.appendChild(link(n)); flat.push(n); } else s.appendChild(document.createTextNode(n.t));
        d.appendChild(s);
        d.appendChild(tree(n.c, depth + 1));
        li.appendChild(d);
      } else if (n.h) {
        li.appendChild(link(n));
        flat.push(n);
      } else {
        // No page yet (a to-do item): plain text, not a link, not in Prev/Next
        li.appendChild(el('span', 'todo', n.t));
      }
      ul.appendChild(li);
    });
    return ul;
  }

  if (side && window.SITE) {
    var nav = el('nav', 'site');
    nav.setAttribute('aria-label', 'Site');
    nav.appendChild(el('div', 'label', window.SITE.title || 'Pages'));
    if (window.SITE.home) { var h = { t: window.SITE.home.t, h: window.SITE.home.h }; var ul0 = el('ul'); var li0 = el('li');
      li0.appendChild(link(h)); ul0.appendChild(li0); nav.appendChild(ul0); flat.push(h); }
    nav.appendChild(tree(window.SITE.pages || [], 0));
    side.insertBefore(nav, side.firstChild);
  }

  // contents of this page, from h2/h3 with an id
  var heads = document.querySelectorAll('main h2[id], main h3[id]');
  var links = {};
  var toc = null;
  if (side && heads.length) {
    toc = el('nav', 'toc');
    toc.setAttribute('aria-label', 'Contents');
    toc.appendChild(el('div', 'label dim', 'On this page'));
    var ol = el('ol');
    heads.forEach(function (hd) {
      var li = el('li');
      var a = el('a', hd.tagName === 'H3' ? 'lv3' : null);
      // Keep words with a hyphen ("-O", "--target") on one line: browsers may break
      // right after a hyphen, which leaves a lone "O" on the next line.
      hd.textContent.split(/(\S*-\S*)/).forEach(function (part, i) {
        if (!part) return;
        a.appendChild(i % 2 ? el('span', 'nobr', part) : document.createTextNode(part));
      });
      a.href = '#' + hd.id;
      links[hd.id] = a;
      li.appendChild(a);
      ol.appendChild(li);
    });
    toc.appendChild(ol);
  }

  // Wide screens: an outline column on the right of the text, always there (empty
  // when the page has no headings) so every page keeps the same column widths.
  // Narrower: the contents go back in the left sidebar (three columns wouldn't fit).
  if (side) {
    var layout = side.parentNode;
    var outline = el('aside', 'side outline');
    var wide = window.matchMedia('(min-width: 1400px)');
    var place = function () {
      if (wide.matches) {
        if (toc) outline.appendChild(toc);
        if (!outline.parentNode) layout.appendChild(outline);
        layout.classList.add('has-outline');
      } else {
        if (toc) side.appendChild(toc);
        if (outline.parentNode) layout.removeChild(outline);
        layout.classList.remove('has-outline');
      }
    };
    place();
    if (wide.addEventListener) wide.addEventListener('change', place); else wide.addListener(place);
  }

  // scroll-spy: highlight the section being read
  var current = null;
  if ('IntersectionObserver' in window) {
    var spy = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (!e.isIntersecting || !links[e.target.id]) return;
        if (current) current.classList.remove('on');
        current = links[e.target.id];
        current.classList.add('on');
      });
    }, { rootMargin: '0px 0px -70% 0px' });
    heads.forEach(function (hd) { spy.observe(hd); });
  }

  // prev / next in site order
  var pager = document.querySelector('.pager');
  if (pager && flat.length) {
    var i = -1;
    flat.forEach(function (n, k) { if (n.h === here) i = k; });
    if (i >= 0) {
      [[flat[i - 1], 'prev', 'Previous'], [flat[i + 1], 'next', 'Next']].forEach(function (p) {
        if (!p[0]) { pager.appendChild(el('span')); return; }
        var a = el('a', p[1]);
        a.href = root + p[0].h;
        a.appendChild(el('small', null, p[2]));
        a.appendChild(document.createTextNode(p[0].t));
        pager.appendChild(a);
      });
    }
  }
})();
