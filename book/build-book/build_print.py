#!/usr/bin/env python3
"""Build the print interior PDF.

  python3 build_print.py <manuscript-dir> <out-dir>

Steps: pandoc -> HTML fragment; assemble front matter, contents and back
matter; render with Chromium; insert blank versos so that every part, chapter
and front-matter section opens on a recto; fill in contents page numbers and
build the index from the rendered text; stamp running heads and folios; set
roman/arabic page labels and document metadata.
"""
import html, json, re, shutil, subprocess, sys
from pathlib import Path
import yaml
from bs4 import BeautifulSoup
import preprocess
import pypdf
from pypdf.generic import NameObject

HERE = Path(__file__).resolve().parent
MS, OUT = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
# --color: colour figures and accents, for the reading PDF; the default is the
# black-and-white print interior.
COLOR = "--color" in sys.argv[3:]
NAME = "interior-color" if COLOR else "interior"
FIGDIR = "figures" if COLOR else "figures-gray"
OUT.mkdir(parents=True, exist_ok=True)
CFG = yaml.safe_load((HERE / "edition.yaml").read_text())
E = html.escape
FF = "".join(f'@font-face {{ font-family: "InterT"; font-weight: {w}; src: url(fonts/inter-latin-{w}-normal.woff2); }}' for w in (400, 500, 600, 700, 800, 900))

def run(*cmd, **kw):
    subprocess.run(cmd, check=True, **kw)

# ---------------------------------------------------------------- body
def pandoc_body():
    ms = preprocess.prepare(MS, OUT / "ms", CFG)
    files = sorted(str(p) for p in ms.glob("*.md"))
    out = OUT / "body-fragment.html"
    run("pandoc", *files, "-f", "markdown", "-t", "html5", "--lua-filter", str(HERE / "chapnum.lua"),
        "--highlight-style=monochrome", "-o", str(out))
    return out.read_text()

def heading_text(tag):
    """Visible heading text without the Chapter/Part label."""
    t = tag.get("data-short")
    return t if t else tag.get_text(" ", strip=True)

def label_of(tag):
    s = tag.find("span", class_=["chapnum", "partnum"])
    return s.get_text(strip=True) if s else ""

# ---------------------------------------------------------------- front matter
def pub_month(ym):
    import datetime
    y, m = (int(x) for x in str(ym).split("-")[:2])
    return datetime.date(y, m, 1).strftime("%B %Y")

def isbn_line(v):
    return E(v) if v else "to be assigned"

def front_matter():
    c = CFG
    pub = E(c["publisher"])
    site = f"<p><strong>Code and errata:</strong> {E(preprocess.code_url(c))}</p>"
    return f"""
<div class="page halftitle">{E(c['title'])}</div>
<div class="blank"></div>
<div class="page titlepg">
  <div class="t">{E(c['title']).replace(' ', '<br/>', 1) if len(c['title']) > 14 else E(c['title'])}</div>
  <div class="s">{E(c['subtitle'])}</div>
  <div class="tag">{E(c['tagline'])}</div>
  <div class="rule"></div>
  <div class="a">{E(c['author'])}</div>
  <div class="pub">{pub}</div>
</div>
<div class="page copyright">
  <p class="bt">{E(c['title'])}<br/><span style="font-weight:500">{E(c['subtitle'])}</span></p>
  <p>Copyright © {E(c['year'])} {E(c['author'])}. All rights reserved.</p>
  <p>No part of this book may be reproduced, stored in a retrieval system or transmitted in any form or by any means,
  electronic, mechanical, photocopying, recording or otherwise, without the prior written permission of the author,
  except for brief quotations in reviews and other uses permitted by copyright law.</p>
  <p><strong>Companion code.</strong> The source code of the <code>llds</code> companion project is licensed under the
  {E(c['code_licence'])}; see the <code>LICENSE</code> file in the project. You may use it in your own programs without
  asking permission.</p>
  {site}
  <p><strong>Disclaimer.</strong> The information in this book is provided “as is”, without warranty of any kind. The
  author shall have no liability for any loss or damage caused, or alleged to be caused, directly or indirectly by the
  instructions or code it contains. The medical-device and PET examples are engineering illustrations: nothing in this book is
  fit for clinical use, constitutes regulatory advice, or describes the products of any manufacturer.
  Performance figures were measured on the machines described in the text; your results will differ.</p>
  <p><strong>Trademarks.</strong> Intel and Xeon are trademarks of Intel Corporation. Arm is a trademark of Arm Limited.
  Linux is a registered trademark of Linus Torvalds. Other product and company names are trademarks of their
  respective owners and are used in an editorial fashion, with no intention of infringement.</p>
  <dl>
    <dt>Edition</dt><dd>{E(c['edition'])}, {E(pub_month(c['pub_date']))} (version {E(c['version'])})</dd>
    <dt>ISBN (paperback)</dt><dd>{isbn_line(c.get('isbn_print'))}</dd>
    <dt>ISBN (ebook)</dt><dd>{isbn_line(c.get('isbn_ebook'))}</dd>
    <dt>Publisher</dt><dd>{pub}</dd>
  </dl>
  <p>Typeset in Source Serif 4, Inter and JetBrains Mono.</p>
</div>
"""

def toc_html(entries, brief, pages):
    """entries: list of (level, id, label, title). pages: id -> folio string."""
    out = [f'<h1 class="fm-head recto" id="{"brief-contents" if brief else "contents"}">'
           f'{"Brief Contents" if brief else "Contents"}</h1>', f'<nav class="toc{" brief" if brief else ""}"><ol>']
    for lvl, hid, label, title in entries:
        if brief and lvl == 3:
            continue
        pg = pages.get(hid, "000")
        num = f'<span class="num">{E(label.replace("Chapter ", "").replace("Appendix ", ""))}</span>' if (lvl == 2 and label) else ""
        if lvl == 1 and label:
            title = f"{label}. {title}"
        out.append(f'<li class="l{lvl}"><a class="row" href="#{hid}"><span class="lbl">{num}{E(title)}</span>'
                   f'<span class="dots"></span><span class="pg">{pg}</span></a></li>')
    out.append("</ol></nav>")
    return "\n".join(out)

def back_matter(index_html):
    parts = [f'<h1 class="fm-head recto" id="index">Index</h1>{index_html}']
    if CFG.get("author_bio"):
        parts.append(f'<h1 class="fm-head recto" id="about">About the Author</h1><div class="about"><p>{E(CFG["author_bio"])}</p></div>')
    parts.append(f'<p class="colophon">{E(CFG["title"])} was written in Markdown, converted with Pandoc and typeset with '
                 f'Chromium. The body text is Source Serif 4, headings are Inter, and code is JetBrains Mono.</p>')
    return "\n".join(parts)

# ---------------------------------------------------------------- assemble
def assemble(body_soup, toc_entries, pages, index_html, blanks):
    body = str(body_soup)
    for hid in blanks:   # blank verso before a recto opener
        body = re.sub(rf'(<h[12][^>]*\bid="{re.escape(hid)}")', r'<div class="blank"></div>\1', body, count=1)
    fm = front_matter()
    tocs = toc_html(toc_entries, True, pages) + toc_html(toc_entries, False, pages)
    for hid in blanks:
        tocs = tocs.replace(f'<h1 class="fm-head recto" id="{hid}"', f'<div class="blank"></div><h1 class="fm-head recto" id="{hid}"')
    back = back_matter(index_html)
    for hid in blanks:
        back = back.replace(f'<h1 class="fm-head recto" id="{hid}"', f'<div class="blank"></div><h1 class="fm-head recto" id="{hid}"')
    return f"""<!DOCTYPE html><html lang="{CFG['language']}"><head><meta charset="utf-8"/>
<title>{E(CFG['title'])}</title><link rel="stylesheet" href="print.css"/>{"<link rel=\"stylesheet\" href=\"print-color.css\"/>" if COLOR else ""}</head><body>
{fm}{tocs}{body}{back}</body></html>"""

def prepare_body(frag):
    soup = BeautifulSoup(frag, "html.parser")
    # part intro paragraphs (between a part heading and the next heading)
    for h in soup.find_all("h1", class_="part"):
        sib = h.find_next_sibling()
        while sib is not None and sib.name not in ("h1", "h2"):
            if sib.name == "p":
                sib["class"] = sib.get("class", []) + ["partintro"]
            sib = sib.find_next_sibling()
    inline_figures(soup)
    entries, openers = [], []
    for h in soup.find_all(["h1", "h2", "h3"]):
        cls = h.get("class", [])
        hid = h.get("id")
        if h.name == "h1":
            entries.append((1, hid, label_of(h), heading_text(h)))
            openers.append(hid)
        elif h.name == "h2" and "chapter" in cls:
            entries.append((2, hid, label_of(h), heading_text(h)))
            openers.append(hid)
        elif h.name == "h3" and hid:
            entries.append((3, hid, "", h.get_text(" ", strip=True)))
    # the preface counts as front matter: show it as a level-2 row in the contents
    entries = [(2, e[1], "", e[3]) if e[1] == "preface" else e for e in entries]
    return soup, entries, openers, collect_sections(soup)

def inline_figures(soup):
    """Replace <img src="figures/x.svg"> with the SVG itself, so the page fonts apply."""
    for n, img in enumerate(soup.find_all("img")):
        src = img.get("src", "")
        if not src.startswith("figures/") or not src.endswith(".svg"):
            continue
        svg = (HERE / FIGDIR / src[len("figures/"):]).read_text()
        svg = re.sub(r'id="arr(\w*)"', rf'id="arr\1_{n}"', svg)
        svg = re.sub(r'url\(#arr(\w*)\)', rf'url(#arr\1_{n})', svg)
        img.replace_with(BeautifulSoup(svg, "html.parser"))

NOT_INDEXED = {"preface", "appC", "appD", "references"}

def collect_sections(soup):
    """Sections in document order: (id, chapter id, heading text, body text).
    A section is an h3 and what follows it, or a chapter's opening before its
    first h3; pandoc emits headings and blocks as siblings, so one pass works."""
    secs, cur, chap = [], None, None
    for el in soup.children:
        name = getattr(el, "name", None)
        if name is None:
            continue
        cls = el.get("class", []) or []
        if name in ("h1", "h2", "h3") and el.get("id") and (name != "h2" or "chapter" in cls):
            if name in ("h1", "h2"):
                chap = el["id"]
            cur = [el["id"], chap, el.get_text(" ", strip=True), []]
            secs.append(cur)
        elif cur is not None:
            cur[3].append(el.get_text(" "))
    return [(a, b, c, re.sub(r"\s+", " ", " ".join(d))) for a, b, c, d in secs]

# ---------------------------------------------------------------- render & inspect
def render(doc, name, outline=True):
    h = OUT / f"{name}.html"
    h.write_text(doc)
    for f in ("print.css", "print-color.css"):
        shutil.copy(HERE / f, OUT / f)
    shutil.copytree(HERE / "fonts", OUT / "fonts", dirs_exist_ok=True)
    pdf = OUT / f"{name}.pdf"
    _TEXT.pop(str(pdf), None)
    run("node", str(HERE / "render.js"), str(h), str(pdf), "--outline" if outline else "")
    return pdf

def dest_pages(pdf):
    r = pypdf.PdfReader(str(pdf))
    m = {}
    for name, d in r.named_destinations.items():
        try:
            m[name.lstrip("/")] = r.get_destination_page_number(d)
        except Exception:
            pass
    def walk(items):   # sections nothing links to are found through the outline
        for o in items:
            if isinstance(o, list):
                walk(o)
            elif o.title.strip() in ("Brief Contents", "Contents"):
                m.setdefault(o.title.strip().lower().replace(" ", "-"), r.get_destination_page_number(o))
    walk(r.outline)
    return r, m

# ---------------------------------------------------------------- index
def load_terms():
    terms = []
    for line in (HERE / "index_terms.txt").read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        sub = line.startswith("  ")
        parts = [p.strip() for p in line.split("|")]
        disp, pats = parts[0], parts[1:] or [re.escape(parts[0])]
        rx = re.compile("|".join(p.replace(" ", r"[\s-]+") for p in pats), re.I)
        if sub and terms:
            terms[-1][2].append((disp, rx))
        else:
            terms.append((disp, rx, []))
    return terms

_TEXT = {}
def all_texts(pdf):
    pdf = str(pdf)
    if pdf not in _TEXT:
        out = subprocess.run(["pdftotext", "-enc", "UTF-8", pdf, "-"], check=True, capture_output=True, text=True).stdout
        _TEXT[pdf] = out.split("\f")
    return _TEXT[pdf]

def page_texts(reader, first, last):
    pages = all_texts(OUT / "interior-pass.pdf")
    txt = {}
    for i in range(first, last):
        t = pages[i]
        t = re.sub(r"(\w)-\n(\w)", r"\1\2", t)
        txt[i] = re.sub(r"\s+", " ", t)
    return txt

def pick_pages(rx, txt, maxn=8):
    counts = {i: len(rx.findall(t)) for i, t in txt.items()}
    hits = [i for i, n in counts.items() if n]
    if len(hits) > maxn:   # keep the densest discussions
        hits = sorted(sorted(hits, key=lambda i: -counts[i])[:maxn])
    return hits

def ranges(pages, folio):
    pages = sorted(set(pages))
    out, i = [], 0
    while i < len(pages):
        j = i
        while j + 1 < len(pages) and pages[j + 1] == pages[j] + 1:
            j += 1
        out.append(folio(pages[i]) if i == j else f"{folio(pages[i])}–{folio(pages[j])}")
        i = j + 1
    return ", ".join(out)

def index_key(disp):
    k = disp.replace("`", "")
    k = re.sub(r"^std::", "", k)
    return re.sub(r"^[^A-Za-z0-9]+", "", k).lower()

def index_label(disp):
    if disp.startswith("`") and disp.endswith("`"):
        return f"<code>{E(disp.strip('`'))}</code>"
    return E(disp)

def term_locations(rx, sections, dp, folio, maxn):
    scored = []
    for k, (sid, chap, head, text) in enumerate(sections):
        if chap in NOT_INDEXED or sid not in dp:
            continue
        in_head = bool(rx.search(head))
        n = len(rx.findall(text))
        if in_head or n >= 3:
            scored.append(((1000 if in_head else 0) + n, k))
    best = sorted(sorted(scored, reverse=True)[:maxn], key=lambda t: t[1])
    spans = []
    for _, k in best:
        sid = sections[k][0]
        start = dp[sid]
        nxt = next((dp[s[0]] for s in sections[k + 1:] if s[0] in dp), start)
        end = max(start, nxt - 1) if nxt > start else start
        if spans and start <= spans[-1][1] + 1:
            spans[-1] = (spans[-1][0], max(end, spans[-1][1]), spans[-1][2])
        else:
            spans.append((start, end, sid))
    return [(f'<a href="#{sid}">{folio(a)}</a>' if a == b else f'<a href="#{sid}">{folio(a)}–{folio(b)}</a>')
            for a, b, sid in spans]

def build_index(sections, dp, folio):
    rows, letter, entries = [], None, 0
    for disp, rx, subs in sorted(load_terms(), key=lambda t: index_key(t[0])):
        locs = term_locations(rx, sections, dp, folio, 6)
        subrows = [(d, term_locations(r, sections, dp, folio, 4)) for d, r in subs]
        subrows = [(d, l) for d, l in subrows if l]
        if not locs and not subrows:
            continue
        key = index_key(disp)
        L = key[0].upper() if key[:1].isalpha() else "Symbols and numbers"
        if L != letter:
            rows.append(f'<div class="letter">{L}</div>')
            letter = L
        rows.append(f'<p class="e">{index_label(disp)}{", " + ", ".join(locs) if locs else ""}</p>')
        entries += 1
        for d, l in subrows:
            rows.append(f'<p class="sub">{index_label(d)}, {", ".join(l)}</p>')
            entries += 1
    intro = ('<p class="index-intro">Page numbers point to the sections that discuss a topic, not to passing mentions. '
             'Library and standard-library names are set in code type and alphabetised without <code>std::</code>.</p>')
    return intro + '<div class="index">' + "\n".join(rows) + "</div>", entries

# ---------------------------------------------------------------- folios
def to_roman(n):
    vals = [(10, "x"), (9, "ix"), (5, "v"), (4, "iv"), (1, "i")]
    s = ""
    for v, r in vals:
        while n >= v:
            s += r; n -= v
    return s

def main():
    if not (HERE / "figures").exists():
        run(sys.executable, str(HERE / "figures.py"), str(HERE / "figures"))
    if not (HERE / "figures-gray").exists():
        run(sys.executable, str(HERE / "figures.py"), str(HERE / "figures-gray"), "--gray")
    soup, entries, openers, sections = prepare_body(pandoc_body())
    entries.append((1, "index", "", "Index"))
    if CFG.get("author_bio"):
        entries.append((1, "about", "", "About the Author"))
    recto_ids = ["brief-contents", "contents"] + openers + ["index"] + (["about"] if CFG.get("author_bio") else [])
    body_start_id = next(e[1] for e in entries if e[0] == 1 and e[1] != "preface")
    pages, index_html, blanks = {}, '<div class="index"></div>', []
    for it in range(5):
        doc = assemble(soup, entries, pages, index_html, blanks)
        pdf = render(doc, f"{NAME}-pass")
        reader, dp = dest_pages(pdf)
        missing = [h for h in recto_ids if h not in dp]
        if missing:
            raise SystemExit(f"no destination for {missing[:5]}")
        # recto check: page index must be even (0-based) -> odd page number
        bad = [h for h in recto_ids if dp[h] % 2 == 1]
        body0 = dp[body_start_id]
        folio = lambda i: (str(i - body0 + 1) if i >= body0 else to_roman(i + 1))
        new_pages = {h: folio(p) for h, p in dp.items()}
        new_index, n_entries = build_index(sections, dp, folio)
        print(f"pass {it}: {len(reader.pages)} pages, {len(bad)} verso openers, {n_entries} index entries", flush=True)
        if not bad and new_pages == pages and new_index == index_html:
            break
        # add or remove blank pages: walk openers in order with a running offset
        if bad:
            offset, nb = 0, set(blanks)
            for h in recto_ids:
                if (dp[h] + offset) % 2 == 1:
                    if h in nb:
                        nb.discard(h); offset -= 1
                    else:
                        nb.add(h); offset += 1
            blanks = [h for h in recto_ids if h in nb]
        pages, index_html = new_pages, new_index
    else:
        raise SystemExit("layout did not converge")

    meta = {"dp": dp, "body0": body0, "n": len(reader.pages), "blanks": blanks, "entries": entries}
    stamp(pdf, meta)

# ---------------------------------------------------------------- running heads
def stamp(pdf, meta):
    reader = pypdf.PdfReader(str(pdf))
    n, dp, body0 = meta["n"], meta["dp"], meta["body0"]
    entries = meta["entries"]
    page_of = lambda h: dp.get(h, 10**9)
    # openers: pages that begin a part/chapter/front section -> no running head
    opener_pages = {page_of(e[1]) for e in entries if e[0] in (1, 2)} | {dp[h] for h in ("brief-contents", "contents", "index") if h in dp}
    part_pages = {page_of(e[1]) for e in entries if e[0] == 1 and e[1] not in ("index", "about")}
    # chapter spans (verso head) and section spans (recto head)
    chaps = sorted([(page_of(e[1]), (e[2] + ": " if e[2] else "") + e[3]) for e in entries if e[0] == 2]
                   + [(dp["brief-contents"], "Brief Contents"), (dp["contents"], "Contents"), (dp["index"], "Index")]
                   + ([(dp["about"], "About the Author")] if "about" in dp else []))
    secs = sorted([(page_of(e[1]), e[3]) for e in entries if e[0] == 3])
    def current(spans, i):
        cur = None
        for p, t in spans:
            if p <= i: cur = (p, t)
            else: break
        return cur
    first_toc = dp["brief-contents"]
    folio = lambda i: (str(i - body0 + 1) if i >= body0 else to_roman(i + 1))
    texts = [t.strip() for t in all_texts(pdf)[:n]]
    W, H = CFG["trim_w_in"], CFG["trim_h_in"]
    pages_html = []
    for i in range(n):
        recto = i % 2 == 0
        blank = not texts[i]
        cell = ""
        if i < first_toc or blank or i in part_pages:
            cell = ""
        elif i in opener_pages:
            cell = f'<div class="fb">{folio(i)}</div>'
        else:
            ch = current(chaps, i)
            sec = current(secs, i)
            if recto:
                t = sec[1] if (sec and ch and sec[0] >= ch[0]) else (ch[1] if ch else "")
                cell = f'<div class="rh r"><span class="t">{E(t)}</span><span class="f">{folio(i)}</span></div>'
            else:
                t = ch[1] if ch else ""
                cell = f'<div class="rh v"><span class="f">{folio(i)}</span><span class="t">{E(t)}</span></div>'
        pages_html.append(f'<div class="pg {"recto" if recto else "verso"}">{cell}</div>')
    css = FF + f"""@page {{ size: {W}in {H}in; margin: 0; }}
body {{ margin: 0; font-family: InterT, sans-serif; color: #000; }}
.pg {{ width: {W}in; height: {H}in; position: relative; break-after: page; overflow: hidden; }}
.rh {{ position: absolute; top: 0.5in; display: flex; justify-content: space-between; align-items: baseline;
       font-size: 7.4pt; letter-spacing: 0.06em; border-bottom: 0.4pt solid #000; padding-bottom: 3pt; }}
.recto .rh {{ left: 0.875in; right: 0.625in; }} .verso .rh {{ left: 0.625in; right: 0.875in; }}
.rh .t {{ text-transform: uppercase; font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 85%; }}
.rh .f {{ font-weight: 700; font-size: 8.6pt; letter-spacing: 0; }}
.fb {{ position: absolute; bottom: 0.42in; left: 0; right: 0; text-align: center; font-size: 8.4pt; font-weight: 600; }}
.recto .fb {{ padding-left: 0.25in; }} .verso .fb {{ padding-right: 0.25in; }}"""
    if COLOR:
        css += """.rh { border-bottom: 0.8pt solid #e8a33d; color: #1b3a5c; } .rh .f, .fb { color: #a0600a; }"""
    ov = render(f"<!DOCTYPE html><html><head><meta charset='utf-8'><style>{css}</style></head><body>{''.join(pages_html)}</body></html>",
                f"{NAME}-overlay", outline=False)
    ovr = pypdf.PdfReader(str(ov))
    assert len(ovr.pages) == n, (len(ovr.pages), n)
    w = pypdf.PdfWriter(clone_from=reader)
    for i in range(n):
        w.pages[i].merge_page(ovr.pages[i])
        w.pages[i].compress_content_streams()
    if n % 2:   # printers bind an even number of pages
        w.add_blank_page()
    n = len(w.pages)
    w.set_page_label(0, body0 - 1, style="/r")
    w.set_page_label(body0, n - 1, style="/D", start=1)
    w.add_metadata({"/Title": f"{CFG['title']}: {CFG['subtitle']}", "/Author": CFG["author"],
                    "/Subject": CFG["tagline"], "/Keywords": "; ".join(CFG["keywords"]),
                    "/Creator": "Pandoc + Chromium", "/Producer": "pypdf"})
    w._root_object[NameObject("/PageMode")] = NameObject("/UseOutlines")
    final = OUT / f"{NAME}.pdf"
    w.compress_identical_objects(remove_duplicates=True, remove_unreferenced=True)
    with open(final, "wb") as f:
        w.write(f)
    (OUT / f"{NAME}.json").write_text(json.dumps({"pages": n, "body_start_index": body0, "blanks": meta["blanks"]}, indent=1))
    print(f"wrote {final}: {n} pages, body starts at index {body0}")

if __name__ == "__main__":
    main()
